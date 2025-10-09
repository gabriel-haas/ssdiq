#pragma once
// -------------------------------------------------------------------------------------
#include "Exceptions.hpp"
#include "IoChannel.hpp"
#include "IoOptions.hpp"
#include "IoRequest.hpp"
#include "Raid.hpp"
#include "RequestStack.hpp"
#include "Time.hpp"
#include "Units.hpp"
// -------------------------------------------------------------------------------------
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
// -------------------------------------------------------------------------------------
namespace mean
#define COUNTERS_BLOCK() if (false)
{
// -------------------------------------------------------------------------------------
template <typename TIoEnvironment, typename TIoChannel, typename TImplRequest>
class Raid0Channel : public IoChannel {
   TIoEnvironment& io_env;
   TIoChannel& io_channel;
   IoOptions io_options;
   RequestStack<RaidRequest<TImplRequest>> request_stack;
   u64 pushTimeout = 0;
   int outstanding = 0;
   u64 pushed = 0;
   u64 pushedFromRemote = 0;
   u64 completed = 0;
   // ------------------------------------------------------------------------------------
// #define IO_TRACE_ON
#ifdef IO_TRACE_ON
   struct TraceElement {
      uint8_t type;
      uint32_t addr;
      uint32_t latency;
      uint64_t tsc;
      TraceElement(uint8_t _type, uint32_t _addr, uint64_t _tsc, uint32_t _latency) : type(_type), addr(_addr), latency(_latency), tsc(_tsc) {}
   };
   std::vector<TraceElement> trace;
#endif
   // -------------------------------------------------------------------------------------
   Raid0 raid;
   static const u64 CHUNK_SIZE = 1 * MEBI;
   // -------------------------------------------------------------------------------------
   // RemoteIoChannelClient remote_client;
   // -------------------------------------------------------------------------------------
 public:
   Raid0Channel(TIoEnvironment& io_env, TIoChannel& io_channel, IoOptions io_options, u64 channelId, u64 totalChannels) // TODO
       : IoChannel(io_env.deviceCount()), io_env(io_env), io_channel(io_channel), io_options(io_options), request_stack(io_options.iodepth), raid(io_env.deviceCount(), CHUNK_SIZE) {
#ifdef IO_TRACE_ON
      trace.reserve(100e6);
#endif
      for (int i = 0; i < request_stack.max_entries; i++) {
         request_stack.requests[i].base.write_back_buffer = (char*)io_env.allocIoMemoryChecked(io_options.write_back_buffer_size, 512);
      }
   };
   ~Raid0Channel() override {
      static std::atomic<int> traceThread = 0;
      for (int i = 0; i < request_stack.max_entries; i++) {
         io_env.freeIoMemory(request_stack.requests[i].base.write_back_buffer, io_options.write_back_buffer_size);
      }
#ifdef IO_TRACE_ON
      const int thread = traceThread.fetch_add(1);
      std::cout << "write trace: " << trace.size() << std::endl;
      std::ofstream outFile("trace" + std::to_string(thread) + ".csv");
      for (uint64_t i = 0; i < trace.size(); i++) {
         auto& t = trace[i];
         outFile << i << "," << t.tsc << "," << (int)t.type << "," << t.addr << "," << t.latency << std::endl;
      }
      outFile.close();
#endif
   };
   // -------------------------------------------------------------------------------------
   IoBaseRequest* getIoRequest() override {
      RaidRequest<TImplRequest>* req = nullptr;
      if (!request_stack.popFromFreeStack(req)) {
         return nullptr;
      }
      return &req->base;
   }
   // -------------------------------------------------------------------------------------
   void pushIoRequest(IoBaseRequest* base_req) override {
      const std::size_t offset = offsetof(RaidRequest<TImplRequest>, base);
      char* raid_request_ptr_char = reinterpret_cast<char*>(base_req) - offset; // a bit of a hack
      RaidRequest<TImplRequest>* req = reinterpret_cast<RaidRequest<TImplRequest>*>(raid_request_ptr_char);
      req->base.stats.push_time = readTSC();
      if (req->base.type == IoRequestType::Write && req->base.write_back) {
         req->base.write_back = true;
         assert(req->base.len <= io_options.write_back_buffer_size);
         std::memcpy(req->base.write_back_buffer, req->base.data, req->base.len);
      }
      req->base.out_of_place_addr = req->base.addr;
      pushed++;
      request_stack.pushToSubmitStack(req);
   };
   // -------------------------------------------------------------------------------------
   int registerBuffers(std::vector<std::pair<void*, uint64_t>>& iovec) override {
      return io_channel.registerBuffers(iovec);
   };
   // -------------------------------------------------------------------------------------
   void _push(const IoBaseRequest& usr) override {
      IoBaseRequest* req = getIoRequest();
      if (!req) {
         throw std::logic_error("Cannot push more: free: " + std::to_string(request_stack.free) + " pushed: " + std::to_string(request_stack.pushed) + " max: " + std::to_string(request_stack.max_entries));
      }
      ensure(req);
      req->copyFields(usr);
      pushIoRequest(req);
   }
   // -------------------------------------------------------------------------------------
   int submitable() override {
      /*
     if (remote_client.remote_count > 0) {
        for (int i = 0; i < remote_client.remote_count; i++) {
           if (!remote_client.remotes[i]->submit_ring.empty()) {
              return 1;
           }
        }
     }
       */
      return request_stack.submitStackSize();
   };
   int _submit() override {
      /*
      if (remote_client.remote_count > 0) {
         for (int i = 0; i < remote_client.remote_count; i++) {
            IoBaseRequest* req;
            while (request_stack.free > request_stack.max_entries*0.25 && remote_client.remotes[i]->submit_ring.try_pop(req)) {
               // from remote to local
               push(req->type, req->data, req->addr, req->len, req->innerCallback, req->write_back);
               pushedFromRemote++;
            }
         }
      }
       */
      // look at all pushed requests, calculate raid, push them to below, submit
      RaidRequest<TImplRequest>* req;
      while (request_stack.popFromSubmitStack(req)) {
         int device;
         u64 raidedOffset;
         assert(req->base.len <= CHUNK_SIZE);
         raid.calc(req->base.addr, device, raidedOffset);
         req->base.device = device;
         req->base.offset = raidedOffset;
         req->base.innerCallback.user_data.val.ptr = req;
         req->base.innerCallback.user_data2.val.ptr = this;
         req->base.innerCallback.callback = [](IoBaseRequest* req) {
            auto rr = reinterpret_cast<RaidRequest<TImplRequest>*>(req->innerCallback.user_data.val.ptr);
            rr->base.user.callback(&rr->base);
            auto this_ptr = (Raid0Channel<TIoEnvironment, TIoChannel, TImplRequest>*)req->innerCallback.user_data2.val.ptr;
            auto ch = reinterpret_cast<Raid0Channel<TIoEnvironment, TIoChannel, TImplRequest>*>(this_ptr);
            /*COUNTERS_BLOCK()*/ { ch->counters.handleCompletedReq(*req); /*leanstore::SSDCounters::myCounters().polled[req->device]++;*/ }
            rr->base.stats.completion_time = readTSC();
#ifdef IO_TRACE_ON
            this_ptr->trace.emplace_back((int)rr->base.type, rr->base.addr, nanoFromTsc(rr->base.stats.submit_time), tscDifferenceUs(readTSC(), rr->base.stats.submit_time));
#endif
            if (!rr->base.reuse_request) {
               ch->request_stack.returnToFreeList(rr);
            }
         };
         outstanding++;
         COUNTERS_BLOCK() {
            if (req->base.type == IoRequestType::Write) {
               counters.outstandingWrite++;
            } else if (req->base.type == IoRequestType::Read) {
               counters.outstandingRead++;
            }
         }
         COUNTERS_BLOCK() { /*leanstore::SSDCounters::myCounters().pushed[device]++;*/ }
         COUNTERS_BLOCK() { counters.handleSubmitReq(req->base); }
         req->base.stats.submit_time = readTSC();
         io_channel._push(req);
         __builtin_prefetch(&req->impl, 0, 1);
      }
      return io_channel._submit();
   };
   int _poll(int min = 0) override {
      int ret = io_channel._poll(min);
      outstanding -= ret;
      completed += ret;
      return ret;
   };
   void _printSpecializedCounters(std::ostream& ss) override { io_channel._printSpecializedCounters(ss); };
   bool hasFreeIoRequests() override {
      return !request_stack.full();
   };
   bool readStackFull() override {
      return request_stack.full();
   }
   bool writeStackFull() override {
      return request_stack.free < (request_stack.max_entries * 0.5);
   }
   void registerRemoteChannel(RemoteIoChannel* rem) override {
      // remote_client.registerRemote(rem);
   }
   void pushBlocking(IoRequestType type, char* data, s64 addr, u64 len, bool write_back = false) override {
      io_channel.pushBlocking(type, data, addr, len, write_back);
   }
};
} // namespace mean
// -------------------------------------------------------------------------------------
