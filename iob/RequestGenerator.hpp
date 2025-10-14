#pragma once

#include "PatternGen.hpp"

#include "PageState.hpp"
#include "Time.hpp"
#include "Units.hpp"
#include "io/IoInterface.hpp"
#include "io/IoRequest.hpp"

#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <bits/stdint-uintn.h>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <random>
#include <sched.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <utility>
#include <vector>

namespace mean {

struct JobOptions {
   std::string name;

   // file
   std::string filename;
   int fd = 0;
   int64_t totalfilesize = 10 * GIBI;
   int64_t filesize = 10 * GIBI;
   uint64_t offset = 0;

   // io options
   int iodepth = 1;
   int iodepth_batch_complete_min = 0;
   int iodepth_batch_complete_max = 0;
   int64_t bs = 4096;
   int64_t io_size = filesize; // 0 means no restrictions
   float writePercent = 0;     // 0.0 - 1.0
   float rateLimit = 0;
   float totalRate = 0;
   bool exponentialRate = true;
   int threads = 1;
   bool printEverySecond = false;
   string logHash;
   int threadStatsInterval = 60;
   bool latencyThread = false;
   long runtimeLimit = 0;
   long maxPage = 0;
   long ioSize = 0;

   string init;
   double fill;
   bool crc = true;
   bool randomData = true;

   long breakEvery = 0;
   long breakFor = 0;

   int fdatasync = 0;

   bool disableChecks = false;

   bool trackPatternAccess = false;

   bool longConsoleOutput = false;

   // Stats options
   bool enableIoTracing = false;
   bool enableLatenyTracking = true;

   string statsPrefix = "p";

   uint64_t totalBlocks() const {
      return filesize / bs;
   }
   uint64_t offsetBlocks() const {
      return offset / bs;
   }
   uint64_t totalMinusOffsetBlocks() const {
      return (filesize - offset) / bs;
   }

   int64_t io_operations() const {
      if (io_size < 0) {
         return 0;
      }
      return io_size / bs;
   }

   std::string print() const {
      std::ostringstream ss;
      ss << "filesize: " << filesize / (float)GIBI << " GB";
      ss << ", io_size/thr: " << (io_size > 0 ? io_size / (float)GIBI : -1) << " GB";
      ss << ", bs: " << bs;
      ss << ", totalBlocks: " << totalBlocks();
      ss << ", writePercent: " << writePercent;
      ss << ", iodepth: " << iodepth;
      ss << ", iodepth_batch_complete_min: " << iodepth_batch_complete_min;
      ss << ", iodepth_batch_complete_max: " << iodepth_batch_complete_max;
      ss << ", fdatasync: " << fdatasync;
      ss << std::endl;
      return ss.str();
   }
};

struct JobStats {
   const u64 bs;

   float time;                    // s
   long readTotalTime = 0;        // us
   long readHghPrioTotalTime = 0; // High priority reads not used
   long writeTotalTime = 0;       // us
   long fdatasyncTotalTime = 0;   // us

   atomic<unsigned long> reads = 0;
   unsigned long readsHighPrio = 0;
   atomic<unsigned long> writes = 0;
   unsigned long fdatasyncs = 0;
   ;
   long lastFdatasync = 0;

   int failedReadRequest = 0;

   std::vector<int> iopsPerSecond;             // 10k seconds
   std::vector<int> readsPerSecond;            // 10k seconds
   std::vector<int> writesPerSecond;           // 10k seconds
   static const int maxSeconds = 24 * 60 * 60; // 1 day in seconds
   std::atomic<int> seconds = 0;

   static const int histBuckets = 20000;
   static const int histFrom = 0;
   static const int histTo = 19999;

   Hist<int, int> readHist{histBuckets, histFrom, histTo};
   Hist<int, int> readHpHist{histBuckets, histFrom, histTo};
   Hist<int, int> writeHist{histBuckets, histFrom, histTo};
   Hist<int, int> fdatasyncHist{histBuckets, histFrom, histTo};

   Hist<int, int> readHistEverySecond{histBuckets, histFrom, histTo};
   Hist<int, int> writeHistEverySecond{histBuckets, histFrom, histTo};
   Hist<int, int> fdatasyncHistEverySecond{histBuckets, histFrom, histTo};

   Hist<int, int> cycleHistEverySecond{histBuckets, histFrom, histTo};

   JobStats(u64 bs) : bs(bs), iopsPerSecond(maxSeconds), readsPerSecond(maxSeconds), writesPerSecond(maxSeconds) {
      assert(iopsPerSecond.size() == maxSeconds);
   }
   JobStats(const JobStats&) = delete;
   JobStats& operator=(const JobStats&) = delete;

   void printStats(std::string& result, const JobOptions& options, uint64_t localTime, uint64_t lastPrintTime, float timeDiff, bool statsFileExists, int genId, std::string patternString, std::string patternDetails) {
      if (!statsFileExists && lastPrintTime == 0) {
         result = "hash,prefix,id,time,timeDiff,device,filesizeGib,pattern,patternDetails,rate,threadRate,threadStatsInterval,expRate,writePercent,writeMibs,readMibs,writes,reads,fdatasyncs";
         result += ",r,";
         readHistEverySecond.writePercentilesHeader("r", result);
         result += ",w,";
         writeHistEverySecond.writePercentilesHeader("w", result);
         result += ",s,";
         fdatasyncHistEverySecond.writePercentilesHeader("s", result);
         result += ",c,";
         cycleHistEverySecond.writePercentilesHeader("c", result);
         result += "\n";
      }

      // constants
      result += options.logHash;
      result += "," + options.statsPrefix;
      result += "," + std::to_string(genId);
      result += "," + std::to_string(localTime);
      result += "," + std::to_string(timeDiff);
      result += "," + options.filename;
      result += "," + std::to_string(options.filesize / GIBI);
      result += "," + patternString;
      result += ",\"" + patternDetails + "\"";
      result += "," + std::to_string(options.totalRate);
      result += "," + std::to_string(options.rateLimit);
      result += "," + std::to_string(options.threadStatsInterval);
      result += "," + std::to_string(options.exponentialRate);
      result += "," + std::to_string(options.writePercent);

      // per seconds
      auto w = std::reduce(writesPerSecond.begin() + lastPrintTime, writesPerSecond.begin() + localTime);
      auto r = std::reduce(readsPerSecond.begin() + lastPrintTime, readsPerSecond.begin() + localTime);
      result += "," + std::to_string((double)w * options.bs / MEBI / timeDiff);
      result += "," + std::to_string((double)r * options.bs / MEBI / timeDiff);
      result += "," + std::to_string(w / timeDiff);
      result += "," + std::to_string(r / timeDiff);
      result += "," + std::to_string((fdatasyncs - lastFdatasync) / timeDiff);

      // hists
      result += ",r,";
      readHistEverySecond.writePercentiles(result);
      result += ",w,";
      writeHistEverySecond.writePercentiles(result);
      result += ",s,";
      fdatasyncHistEverySecond.writePercentiles(result);
      result += ",c,";
      cycleHistEverySecond.writePercentiles(result);

      // reset hists
      readHistEverySecond.resetData();
      writeHistEverySecond.resetData();
      fdatasyncHistEverySecond.resetData();
      cycleHistEverySecond.resetData();

      lastFdatasync = fdatasyncs;
   }
};

struct IoTrace {
   struct IoStat {
      int threadId;
      int reqId;
      uint64_t begin;
      uint64_t submit;
      uint64_t end;
      u64 addr;
      u32 len;
      IoRequestType type;

      static void dumpIoStatHeader(std::ostream& out, std::string prefix) {
         out << prefix << "threadid,reqId,type, begin, submit, end, addr, len";
      }
      void dumpIoStat(std::string prefix, std::ostream& out) const {
         out << prefix << threadId << "," << reqId << "," << (int)type << "," << nanoFromTsc(begin) << "," << nanoFromTsc(submit) << "," << nanoFromTsc(end) << "," << addr << "," << len;
      }
   };
   std::vector<IoStat> ioTrace;
   bool ioTracing = false;
   uint64_t evaluatedtIos = 0;

   void setIoTracing(bool enable) {
      ioTracing = enable;
      if (enable) {
         ioTrace.resize(10000000);
      }
   }

   static void dumpIoTreaceHeader(std::ostream& out, std::string prefix) {
      IoStat::dumpIoStatHeader(out, prefix);
      out << std::endl;
   }
   void dumpIoTrace(std::ostream& out, std::string prefix = "") {
      for (uint64_t i = 0; i < evaluatedtIos; i++) {
         ioTrace[i].dumpIoStat(prefix, out);
         out << std::endl;
      }
   }
};

class RequestGenerator {
 public:
   std::string name;
   int genId;
   const JobOptions options;
   iob::PatternGen& patternGen;
   FileState& fileState;
   IoTrace ioTrace;

   JobStats stats;

   // data frames
   std::unique_ptr<char*[]> readData;
   std::unique_ptr<char*[]> writeData;
   char* rd;
   char* wd;

   uint64_t lastFsync = 0;
   uint64_t preparedWrites = 0;
   uint64_t preparedReads = 0;
   uint64_t preparedFsyncs = 0;
   const int64_t totalBlocks = options.totalBlocks();
   const int64_t ops = options.io_operations();

   std::atomic<bool> keep_running = true;
   IoChannel& ioChannel;
   atomic<long>& time;
   long localTime = 0;
   long lastPrintTime = 0;

   std::vector<u64> availableReqStack;
   int availableReqStackCnt = 0;

   unsigned long bss = options.totalMinusOffsetBlocks() / options.threads;
   std::uniform_int_distribution<unsigned long> rbs_dist{0, bss};
   unsigned long max_blocks = options.totalMinusOffsetBlocks();

   std::random_device randDevice;
   std::mt19937_64 gen{randDevice()};
   std::exponential_distribution<> rateLimitExpDist;

   std::ofstream statsFile;
   bool statsFileExists = false;

   RequestGenerator(const RequestGenerator& other) = delete;
   RequestGenerator(RequestGenerator&& other) = delete;
   RequestGenerator& operator=(const RequestGenerator&) = delete;
   RequestGenerator& operator=(RequestGenerator&& other) = delete;

   RequestGenerator(std::string name, JobOptions& options, IoChannel& ioChannel, int genId, atomic<long>& time, iob::PatternGen& patternGen, FileState& fileState)
       : name(std::move(name)), genId(genId), options(options), patternGen(patternGen), fileState(fileState), stats(options.bs), ioChannel(ioChannel), time(time), availableReqStack(options.iodepth), rateLimitExpDist(options.rateLimit) {
      ioTrace.setIoTracing(options.enableIoTracing);
      readData = std::make_unique<char*[]>(options.iodepth);
      writeData = std::make_unique<char*[]>(options.iodepth);

      int align = 0;
      // rd = (char*)IoInterface::allocIoMemoryChecked(((options.iodepth + align)*options.bs) + 512, 512) + 512;
      rd = (char*)IoInterface::allocIoMemoryChecked(((options.iodepth + align) * options.bs), 512);
      // std::cout << "rd align 4096: " << (u64)((u64)rd % 4096) << " 512: " <<  (u64)((u64)rd % 512) << std::endl;
      // wd = (char*)IoInterface::allocIoMemoryChecked(((options.iodepth + align)*options.bs) + 512, 512);
      //  WELL, seems like it is slower when not aligned to 4K
      wd = (char*)IoInterface::allocIoMemoryChecked(((options.iodepth + align) * options.bs), 512);
      std::vector<std::pair<void*, uint64_t>> iovec;
      for (int i = 0; i < options.iodepth; i++) {
         readData[i] = rd + ((align + options.bs) * i) + 0;  //(char*) IoInterface::allocIoMemoryChecked(options.bs, 1);
         writeData[i] = wd + ((align + options.bs) * i) + 0; // (char*) IoInterface::allocIoMemoryChecked(options.bs, 1);
         memset(writeData[i], 'B', options.bs);
         memset(readData[i], 'A', options.bs);
         iovec.emplace_back(std::pair(readData[i], options.bs));
         iovec.emplace_back(std::pair(writeData[i], options.bs));
         availableReqStack[i] = i;
      }
      availableReqStackCnt = options.iodepth;
      std::string statsFileName = "iob-stats-id" + to_string(genId) + "-" + options.statsPrefix + ".csv";
      {
         std::ifstream fileExists(statsFileName);
         statsFileExists = fileExists.good();
      }
      statsFile.open(statsFileName, std::ios_base::app);
      // ioChannel.registerBuffers(iovec);
   }

   ~RequestGenerator() {
      IoInterface::freeIoMemory(rd);
      IoInterface::freeIoMemory(wd);
      /*
      for (int i = 0; i < options.iodepth; i++) {
         //IoInterface::freeIoMemory(readData[i]);
         //IoInterface::freeIoMemory(writeData[i]);
      }
      */
   }

   /*
   long __attribute__((optimize("O0"))) busyDelay(long unsigned duration) {
      auto start = getTimePoint();
      long sum = 0;
      do {
         const auto to = random_generator.getRandU64(500, 1000);
         for (u64 i = 0; i < to; i++) {
            sum += i;
         }
      } while (timePointDifference(getTimePoint(), start) < duration);
      return sum;
   }
   */

   void stopIo() {
      keep_running = false;
   }

   int runIo() {
      uint64_t countGets = 0;
      // std::cout << options.name << " ready: ops:" <<  ops << " bs: " << options.bs << std::endl;

      getSeconds();
      auto start = getSeconds();

      int64_t completed = 0;
      long polled = 0;
      long submitted = 0;

      mean::UserIoCallback cb;
      cb.user_data.val.ptr = this;
      cb.user_data2.val.u = options.disableChecks;
      cb.callback = [](mean::IoBaseRequest* req) {
         auto* this_ptr = req->user.user_data.as<RequestGenerator*>();
         if ((req->type == IoRequestType::Write || req->type == IoRequestType::Read) && !req->user.user_data2.val.u) {
            this_ptr->fileState.checkBuffer(req->data, req->addr, req->len);
         }
         this_ptr->evaluateIocb(*req);
         this_ptr->availableReqStack[this_ptr->availableReqStackCnt++] = (req->id);
      };

      long lastOps = 0;
      long lastReads = 0;
      long lastWrites = 0;

      std::string statsStr;
      statsStr.reserve(2048);

      auto lastPrintTimeClock = getSeconds();
      auto nextStartTime = mean::readTSC();
      long longLat = 0;
      auto lastCycle = mean::readTSC();
      while ((ops <= 0 || completed < ops) && keep_running) {
         // do {
         if (options.breakEvery <= 0 || (time + 1) % (options.breakEvery + options.breakFor) < options.breakEvery) { // check if there is a break
            while (availableReqStackCnt > 0 && ((ops <= 0 || completed < ops) && keep_running)) {
               if (options.rateLimit > 0) { // when rate limiting is enabled only add more if needed
                  auto now = mean::readTSC();
                  if (now >= nextStartTime) {
                     if (mean::tscDifferenceS(now, nextStartTime) > 5) {
                        longLat++;
                        nextStartTime = now;
                        std::cout << "reset" << std::endl
                                  << std::flush;
                     }
                     double d = 1 / options.rateLimit;
                     if (options.exponentialRate) {
                        d = rateLimitExpDist(gen);
                     }
                     nextStartTime += mean::nsToTSC(d * 1e9);
                  } else {
                     break;
                  }
               }
               IoBaseRequest reqCpy;
               availableReqStackCnt--;
               reqCpy.id = availableReqStack[availableReqStackCnt];
               reqCpy.user = cb;
               prepareRequest(reqCpy);
               // if request addr is dividable by page size, the submit a trim command first
               if (patternGen.pattern == iob::PatternGen::Pattern::ZNS) {
                  ensure(patternGen.options.znsPagesPerZone * reqCpy.len != 0);
                  if (reqCpy.addr % (patternGen.options.znsPagesPerZone * reqCpy.len) == 0) {
                     int fd = IoInterface::instance().getDeviceInfo().devices[0].fd;
                     uint64_t range[2];
                     range[0] = reqCpy.addr;
                     range[1] = patternGen.options.znsPagesPerZone * patternGen.options.pageSize;
                     std::cout << "BLKDISCARD addr: " << reqCpy.addr << " bytes " << range[1] << std::endl;
                     int r = ioctl(fd, BLKDISCARD, &range);
                     if (r < 0) {
                        std::cout << "BLKDISCARD failed: " << r << " errno: " << errno << std::endl;
                     }
                  }
               }
               ioChannel.push(reqCpy);
               submitted++;
            }
            ioChannel.submit();
         }

         auto doPoll = [&]() {
            int got = ioChannel.poll(); // options.iodepth_batch_complete_min, options.iodepth_batch_complete_max
            completed += got;
            if (got < 0) {
               throw std::logic_error("could not get events. gotEvents is " + std::to_string(got));
            }
            polled += got;
            countGets++;
         };
         doPoll();

         if (time != localTime) {
            localTime = time;

            // per seconds stats
            stats.iopsPerSecond[stats.seconds] = completed - lastOps;
            stats.readsPerSecond[stats.seconds] = stats.reads - lastReads;
            stats.writesPerSecond[stats.seconds] = stats.writes - lastWrites;
            stats.seconds++;
            if (stats.seconds >= JobStats::maxSeconds) {
               throw std::logic_error("over max seconds");
            }
            lastOps = completed;
            lastReads = stats.reads;
            lastWrites = stats.writes;

            // print/hists only every x seconds
            if (localTime - lastPrintTime >= options.threadStatsInterval) {
               auto clockTimeNow = getSeconds();
               auto timeDiff = clockTimeNow - lastPrintTimeClock;
               lastPrintTimeClock = clockTimeNow;
               stats.printStats(statsStr, options, localTime, lastPrintTime, timeDiff, statsFileExists, genId, patternGen.options.patternString, patternGen.patternDetails());
               statsFile << statsStr << std::endl;
               statsStr.clear();
               lastPrintTime = localTime;
            }
         }

         auto nowCycle = mean::readTSC();
         stats.cycleHistEverySecond.increaseSlot(tscDifferenceUs(nowCycle, lastCycle));
         lastCycle = nowCycle;
      }
      stats.time = (getSeconds() - start);

      // done. get the remaining events.
      while (polled < submitted) {
         ioChannel.submit();
         polled += ioChannel.poll();
      }
      std::stringstream ss;
      ioChannel.printCounters(ss);
      ss << endl;
      cout << ss.str() << std::flush;
      return 0;
   }

   float sumFreq = 0;
   std::vector<uint64_t> patternAccess;
   static constexpr int NEXT_SIZE = 1;
   int next_seq_ptr = NEXT_SIZE;
   std::array<uint64_t, NEXT_SIZE> next_seq;
   uint64_t patternGenerator() {
      uint64_t addr;
      if (next_seq_ptr == NEXT_SIZE) {
         for (int i = 0; i < NEXT_SIZE; i++) {
            next_seq[i] = patternGen.accessPatternGenerator(gen);
         }
         next_seq_ptr = 0;
      }
      int64_t block = next_seq[next_seq_ptr++];
      if (options.trackPatternAccess) {
         if (patternAccess.empty()) {
            patternAccess.resize(patternGen.options.logicalPages);
         }
         patternAccess.at(block)++;
      }
      ensure(block >= 0 && block < options.totalMinusOffsetBlocks());
      addr = block * options.bs + options.offset;
      // addr = (block % ((options.filesize - options.offset) / options.io_alignment)) * options.io_alignment + options.offset ;
      return addr;
   }
   static void dumpPatternAccessHeader(std::ostream& out, std::string prefix) {
      out << "idx, access" << endl;
   }
   void aggregatePatternAccess(std::array<long, 100>& hist) {
      if (options.trackPatternAccess) {
         for (unsigned long accesses: patternAccess) {
            ensure(accesses >= 0);
            if (accesses < hist.size()) {
               hist[accesses]++;
            } else {
               hist[hist.size() - 1]++;
            }
         }
      }
   }
   void samplePatternAccess(std::vector<long>& sampleLocs, std::vector<long>& accesses) {
      if (options.trackPatternAccess) {
         cout << "thread [" << genId << "]: ";
         for (uint64_t i = 0; i < sampleLocs.size(); i++) {
            accesses[i] += patternAccess[sampleLocs[i]];
            cout << patternAccess[sampleLocs[i]] << ",";
         }
         cout << endl;
      }
   }
   void dumpPatternAccess(std::string prefix, std::ostream& out) {
      if (options.trackPatternAccess) {
         for (uint64_t i = 0; i < patternAccess.size(); i++) {
            out << i << "," << patternAccess[i] << endl;
         }
      }
   }
   std::random_device randDev;
   std::mt19937_64 mersene{randDev()};
   void prepareRequest(IoBaseRequest& req) {
      if (options.fdatasync > 0 && preparedWrites - lastFsync == (uint64_t)options.fdatasync) {
         // c->aio_lio_opcode = IO_CMD_FDSYNC;
         req.type = IoRequestType::Fsync;
         req.len = 0;
         req.addr = 0;
         req.data = nullptr;
         lastFsync = preparedWrites;
         preparedFsyncs++;
      } else {
         req.len = options.bs;
         assert(std::mt19937_64::min() == 0);
         std::uniform_real_distribution<float> rwDist(0, 1);
         volatile float randrw = rwDist(mersene);
         if (options.writePercent > 0 && randrw <= options.writePercent) {
            // write
            req.addr = patternGenerator();
            // std::cout << req.addr << std::endl;
            // static int cnt = 0;
            // cnt++;
            // if (cnt > 100) {
            // std::terminate();
            //}
            // req.write_back = true;
            req.type = IoRequestType::Write;
            req.data = writeData[req.id];
            // std::cout << "w: " << req.addr << " len: " << req.len << std::endl;
            fileState.writeBufferChecks(req.data, req.addr, req.len, mersene);
            preparedWrites++;
         } else {
            // read
            req.addr = patternGenerator();
            ///
            /*
            unsigned long rbs = rbs_dist(mersene);
            req.addr = (rbs*options.bs/(64*1024))*64*1024*options.threads + genId*64*1024 + (rbs*options.bs % 64*1024);
            std::string bla = "threasd: " + std::to_string(options.threads) + "genId: " + std::to_string(genId) +" addr: " + std::to_string(req.addr);
            std::cout << bla << std::endl;
            //*/
            assert(req.addr + req.len <= options.filesize);
            assert(req.addr % 512 == 0);
            req.type = IoRequestType::Read;
            req.data = readData[req.id];
            preparedReads++;
         }
      }
   }

   uint64_t evaluateIocb(const IoBaseRequest& req) {
      uint64_t sum = 0;
      // ensure(req.device == genId);
      if (options.enableIoTracing) {
         IoTrace::IoStat& ios = ioTrace.ioTrace.at(ioTrace.evaluatedtIos);
         ios.threadId = genId;
         ios.reqId = req.id;
         ios.begin = req.stats.push_time;
         ios.submit = req.stats.submit_time;
         ios.end = req.stats.completion_time;
         ios.type = req.type;
         ios.addr = req.addr;
         ios.len = req.len;
         ioTrace.evaluatedtIos++;
      }
      if (!options.enableLatenyTracking) {
         if (req.type == IoRequestType::Read) {
            stats.reads++;
         } else if (req.type == IoRequestType::Write) {
            stats.writes++;
         } else if (req.type == IoRequestType::Fsync) {
            stats.fdatasyncs++;
         } else {
            raise(SIGTRAP);
         }
      } else {
         const auto thisTime = tscDifferenceUs(readTSC(), req.stats.push_time);
         if (req.type == IoRequestType::Fsync) {
            stats.fdatasyncTotalTime += thisTime;
            stats.fdatasyncHist.increaseSlot(thisTime);
            stats.fdatasyncHistEverySecond.increaseSlot(thisTime);
            stats.fdatasyncs++;
         } else if (req.type == IoRequestType::Read) {
            sum += ((uint64_t*)req.data)[0];
            // if (c->aio_reqprio == 1) {
            //  stats.readHghPrioTotalTime += thisTime;
            //  stats.readHpHist.increaseSlot(thisTime);
            //  stats.readsHighPrio++;
            // } else {
            stats.readTotalTime += thisTime;
            stats.readHist.increaseSlot(thisTime);
            stats.reads++;
            //}
            stats.readHistEverySecond.increaseSlot(thisTime);
            // assert(((char*)(*c).aio_buf)[0] == (char)(*c).aio_offset);
         } else {
            stats.writeTotalTime += thisTime;
            stats.writeHist.increaseSlot(thisTime);
            stats.writeHistEverySecond.increaseSlot(thisTime);
            stats.writes++;
         }
      }
      return sum;
   }
};
}; // namespace mean
