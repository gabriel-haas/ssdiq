#include "LibaioImpl.hpp"
// -------------------------------------------------------------------------------------
#include "../IoOptions.hpp"
#include "Exceptions.hpp"
#include "Units.hpp"
// -------------------------------------------------------------------------------------
#include <cassert>
#include <cstddef>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <libaio.h>
#include <linux/fs.h>
#include <memory>
#include <regex>
#include <string>
#include <sys/ioctl.h>
#include <utility>
// -------------------------------------------------------------------------------------
namespace mean {
// -------------------------------------------------------------------------------------
// Linux Base Env
// -------------------------------------------------------------------------------------
LinuxBaseEnv::~LinuxBaseEnv() {
   raidCtl->forEach([]([[maybe_unused]] std::string& dev, int& fd) {
      close(fd);
      fd = -1;
   });
}
std::string fileToNGDevice(std::string dev) {
   std::filesystem::path p = dev;
   std::cout << "dev: " << dev;
   while (std::filesystem::is_symlink(p)) {
      p = std::filesystem::read_symlink(p);
      std::cout << " -> " << p;
   }
   if (p.string().find("nvme") == std::string::npos &&
       p.string().find("ng") == std::string::npos) {
      ensurem(false, "\"" + dev + "\" not a block or character device");
   }
   std::regex r("nvme");
   dev = std::regex_replace(p.string(), r, "ng");
   std::regex r2("../../");
   dev = std::regex_replace(dev, r2, "/dev/");
   std::cout << " -> " << dev << std::endl;
   return dev;
}
void LinuxBaseEnv::init(IoOptions options) {
   this->ioOptions = std::move(options);
   raidCtl = std::make_unique<RaidController<int>>(ioOptions.path);
   raidCtl->forEach([this](std::string& dev, int& fd) {
      int flags = O_RDWR | O_NOATIME;
      if (!ioOptions.ioUringNVMePassthrough) {
         flags |= O_DIRECT;
      } else {
         dev = fileToNGDevice(dev);
      }
      if (this->ioOptions.truncate) {
         flags |= O_TRUNC | O_CREAT;
      }
      // -------------------------------------------------------------------------------------
      fd = open(dev.c_str(), flags, 0600);
      // -------------------------------------------------------------------------------------
      posix_check(fd > -1, "open file failed ");
      ensure(fcntl(fd, F_GETFL) != -1);
      std::cout << "connected to: " << dev << std::endl;
   });
   std::cout << " all connections: [";
   raidCtl->forEach([](std::string& dev, int& fd) {
      std::cout << " " << dev;
   });
   std::cout << "]" << endl;
}
int LinuxBaseEnv::deviceCount() {
   return raidCtl->deviceCount();
   ;
}
u64 LinuxBaseEnv::storageSize() {
   u64 end_of_block_device = 0;
   raidCtl->forEach([&end_of_block_device]([[maybe_unused]] std::string& dev, int& fd) {
      u64 this_end = 0;
      ioctl(fd, BLKGETSIZE64, &this_end);
      if (end_of_block_device == 0) {
         end_of_block_device = this_end;
      }
      if (this_end != end_of_block_device) {
         std::cout << "WARNING: using SSD with different sizes. Using std::min(" << this_end << "," << end_of_block_device << ")" << std::endl;
         end_of_block_device = std::min(this_end, end_of_block_device);
      }
   });
   return end_of_block_device;
};
// -------------------------------------------------------------------------------------
RaidController<int>& LinuxBaseEnv::getRaidCtl() {
   return *raidCtl;
}
DeviceInformation LinuxBaseEnv::getDeviceInfo() {
   DeviceInformation d;
   d.devices.resize(deviceCount());
   for (int i = 0; i < raidCtl->deviceCount(); i++) {
      d.devices[i].id = i;
      d.devices[i].name = raidCtl->name(i);
      d.devices[i].fd = raidCtl->deviceTypeOrFd(i);
   }
   return d;
}
// -------------------------------------------------------------------------------------
// Linux Base Channel
// -------------------------------------------------------------------------------------
LinuxBaseChannel::LinuxBaseChannel(RaidController<int>& raidCtl, IoOptions ioOptions) : raidCtl(raidCtl), ioOptions(std::move(ioOptions)) {
}
void* LinuxBaseEnv::allocIoMemoryChecked(size_t size, size_t align) {
   auto* mem = allocIoMemory(size, align);
   null_checkm(mem, "Memory allocation failed");
   return mem;
};
void* LinuxBaseEnv::allocIoMemory(size_t size, [[maybe_unused]] size_t align) {
   void* bfs = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   madvise(bfs, size, MADV_HUGEPAGE);
   madvise(bfs, size,
           MADV_DONTFORK); // O_DIRECT does not work with forking.
   return bfs;
   // return std::aligned_alloc(align, size);
}
void LinuxBaseEnv::freeIoMemory(void* ptr, size_t size) {
   munmap(ptr, size);
   // std::free(ptr);
}
void LinuxBaseChannel::pushBlocking(IoRequestType type, char* data, u64 offset, u64 len, [[maybe_unused]] bool write_back) {
   int* fd;
   u64 raidedOffset;
   raidCtl.calc(offset, len, fd, raidedOffset);
   switch (type) {
   case IoRequestType::Read: {
      s64 ok = pread(*fd, data, len, (s64)raidedOffset);
      posix_check(ok);
      ensurem(ok == (s64)len, "I/O error: " + to_string(ok) + " (expected: " + to_string(len) + ")");
      break;
   }
   case IoRequestType::Write: {
      s64 ok = pwrite(*fd, data, len, (s64)raidedOffset);
      posix_check(ok);
      ensurem(ok == (s64)len, "I/O error: " + to_string(ok) + " (expected: " + to_string(len) + ")");
      break;
   }
   default:
      throw std::logic_error("not implemented");
   }
}
// -------------------------------------------------------------------------------------
// Libaio Env
// -------------------------------------------------------------------------------------
LibaioChannel& LibaioEnv::getIoChannel(int channel) {
   auto ch = channels.find(channel);
   if (ch == channels.end()) {
      ch = channels.insert({channel, std::make_unique<LibaioChannel>(*raidCtl, ioOptions)}).first;
   }
   return *ch->second;
}
// -------------------------------------------------------------------------------------
// Libaio Channel
// -------------------------------------------------------------------------------------
LibaioChannel::LibaioChannel(RaidController<int>& raidCtl, const IoOptions& ioOptions) : LinuxBaseChannel(raidCtl, ioOptions) {
   // -------------------------------------------------------------------------------------
   const int ret = io_setup(ioOptions.iodepth, &aio_context);
   if (ret != 0) {
      throw std::logic_error("io_setup failed, ret code = " + std::to_string(ret) +
                             "\nIff ret code is 11/EAGAIN, this could be cause by a too high iodepth. " +
                             "See: /proc/sys/fs/aio-max-nr for max number of aio events that can be used.");
   }
   // -------------------------------------------------------------------------------------
   request_stack.reserve(ioOptions.iodepth);
   events = std::make_unique<struct io_event[]>(ioOptions.iodepth);
}
// -------------------------------------------------------------------------------------
LibaioChannel::~LibaioChannel() {
   io_destroy(aio_context);
}
// -------------------------------------------------------------------------------------
void LibaioChannel::_push(RaidRequest<LibaioIoRequest>* req) {
   switch (req->base.type) {
   case IoRequestType::Write:
      io_prep_pwrite(&req->impl.aio_iocb, raidCtl.device(req->base.device), req->base.buffer(), req->base.len, req->base.offset);
      break;
   case IoRequestType::Read:
      io_prep_pread(&req->impl.aio_iocb, raidCtl.device(req->base.device), req->base.buffer(), req->base.len, req->base.offset);
      // std::cout << "read: " << req->aio_fildes << " len: " << req->u.c.nbytes << " addr: " << req->u.c.offset << std::endl;
      break;
   default:
      throw "";
   }
   request_stack.push_back(reinterpret_cast<iocb*>(req));
}
// -------------------------------------------------------------------------------------
void LibaioChannel::_printSpecializedCounters(std::ostream& ss) {
   ss << "aio: ";
}
// -------------------------------------------------------------------------------------
int LibaioChannel::_submit() {
   /*
   if(rand() % 1000000 == 0)
      printf("len: %i thr: %p init: %p cnt: %lu \n", request_stack.data()[0]->u.saddr.len, (void*)pthread_self(), &aio_context, request_stack.size());
      */
   int submitted = io_submit(aio_context, static_cast<long>(request_stack.size()), reinterpret_cast<iocb**>(request_stack.data()));
   ensure(request_stack.size() == (u64)submitted);
   outstanding += static_cast<int>(request_stack.size());
   request_stack.clear();
   return submitted;
}
// -------------------------------------------------------------------------------------
int LibaioChannel::_poll(int min) {
   // ensure(outstanding <= ioOptions.iodepth);
   int done_requests = 0;
   do {
      done_requests = io_getevents(aio_context, 0, outstanding, events.get(), nullptr);
   } while (done_requests == -EINTR); // interrupted by user, e.g. in gdb
   ensure(done_requests >= 0);
   outstanding -= done_requests;
   // std::cout << "polled: outstanding: " << request_stack->outstanding() << " done: " << done_requests << " outstanding: " <<
   // request_stack->outstanding() << std::endl << std::flush; ensure(done_requests == request_stack->outstanding());
   for (int i = 0; i < done_requests; i++) {
      auto& event = events[i];
      ensure(event.res2 == 0);
      // ensure(event.res == ioOptions.write_back_buffer_size);
      auto* req = reinterpret_cast<RaidRequest<LibaioIoRequest>*>(event.obj);
      if (event.res != req->base.len) {
         req->base.print(std::cout);
         throw std::logic_error("libaio event.res != len: event.res: " + std::to_string((long)event.res) + " len: " + std::to_string(req->base.len));
      }
      // eunsure(((char*)req->u.c.buf)[0] == (char)(req->u.c.offset/(16*1024)));
      req->base.innerCallback.callback(&req->base);
      // std::cout << "poll done " << event.res << "r2: " << event.res2 << " time: " <<
      // std::chrono::duration_cast<std::chrono::microseconds>(req->base.completion_time - req->base.push_time).count() << std::endl;
   }
   return done_requests;
}
// -------------------------------------------------------------------------------------
} // namespace mean
// -------------------------------------------------------------------------------------
