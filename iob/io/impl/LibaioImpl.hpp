#pragma once
// -------------------------------------------------------------------------------------
#include "../DeviceInformation.hpp"
#include "../IoOptions.hpp"
#include "../IoRequest.hpp"
#include "../Raid.hpp"
// -------------------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <libaio.h>
#include <memory>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>
// -------------------------------------------------------------------------------------
namespace mean {
// -------------------------------------------------------------------------------------
// Common Linux code / shared by liabio and liburing
// -------------------------------------------------------------------------------------
class LinuxBaseEnv {
 protected:
   std::unique_ptr<RaidController<int>> raidCtl;
   IoOptions ioOptions;

 public:
   ~LinuxBaseEnv();
   void init(IoOptions options);
   int deviceCount();
   u64 storageSize();
   // -------------------------------------------------------------------------------------
   RaidController<int>& getRaidCtl();
   // -------------------------------------------------------------------------------------
   void* allocIoMemory(size_t size, size_t align);
   void* allocIoMemoryChecked(size_t size, size_t align);
   void freeIoMemory(void* ptr, size_t size = 0);
   DeviceInformation getDeviceInfo();
};
class LinuxBaseChannel {
 protected:
   RaidController<int>& raidCtl;
   IoOptions ioOptions;

 public:
   LinuxBaseChannel(RaidController<int>& raidCtl, IoOptions ioOptions);
   void pushBlocking(IoRequestType type, char* data, u64 offset, u64 len, bool write_back);
   // -------------------------------------------------------------------------------------
   void printCounters(std::ostream& ss);
   // -------------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
// Libaio
// -------------------------------------------------------------------------------------
class LibaioChannel;
class LibaioEnv : public LinuxBaseEnv {
 public:
   std::unordered_map<int, std::unique_ptr<LibaioChannel>> channels;
   // -------------------------------------------------------------------------------------
   LibaioChannel& getIoChannel(int channel);
};
// -------------------------------------------------------------------------------------
struct LibaioIoRequest {
   struct iocb aio_iocb; // must be first
};
static_assert(offsetof(LibaioIoRequest, aio_iocb) == 0, "Required so pointers can be used as iocb pointers");
static_assert(offsetof(RaidRequest<LibaioIoRequest>, impl) == 0, "Required so pointers can be used as iocb pointers");
class LibaioChannel : public LinuxBaseChannel {
   io_context_t aio_context = nullptr;
   std::vector<iocb*> request_stack;
   int outstanding = 0;
   std::unique_ptr<struct io_event[]> events;

 public:
   LibaioChannel(RaidController<int>& raidCtl, const IoOptions& ioOptions);
   ~LibaioChannel();
   // -------------------------------------------------------------------------------------
   void _push(RaidRequest<LibaioIoRequest>* req);
   int _submit();
   int _poll(int min = 0);
   void _printSpecializedCounters(std::ostream& ss);
   int registerBuffers(std::vector<std::pair<void*, uint64_t>>& buffers) {
      return 0;
   }
};
// -------------------------------------------------------------------------------------
} // namespace mean
// -------------------------------------------------------------------------------------
