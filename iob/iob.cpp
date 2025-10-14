// -------------------------------------------------------------------------------------
#include "Env.hpp"
#include "PageState.hpp"
#include "PatternGen.hpp"
#include "RequestGenerator.hpp"
#include "ThreadBase.hpp"
#include "Time.hpp"
#include "Units.hpp"
#include "io/IoInterface.hpp"
#include "io/impl/NvmeLog.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <thread>

static void initializeSSDIfNecessary(mean::FileState& fileState, long maxPage, long bufSize, const std::string& init, int iodepth) {
   using namespace mean;
   // check if necessary
   IoChannel& ioChannel = IoInterface::instance().getIoChannel(0);

   int initBufSize = 512 * 1024; // FIXME: this might not work with SPDK on some SSDs, check for max transfer size and use that.
   char* buf = (char*)IoInterface::instance().allocIoMemoryChecked(initBufSize + 512, 512);
   memset(buf, 0, initBufSize);

   long iniOps = ((maxPage * bufSize) / initBufSize) + 1; // +1 because you need to round up
   bool forceInit = init == "yes";
   bool autoInit = init == "auto";
   bool disableCheck = init == "disable" || init == "no";

   int iniDoneCheck = true;
   if (!forceInit && !disableCheck) {
      // just a heurisitc: check first and last page
      // TODO: refactor: move this to FileState?
      auto offset = 0 * bufSize;
      ioChannel.pushBlocking(IoRequestType::Read, buf, offset, bufSize);
      const bool check1 = fileState.checkBufferNoThrow(buf, offset, bufSize);
      std::cout << "check 0: " << check1 << std::endl;
      iniDoneCheck &= check1;
      fileState.resetBufferChecks(buf, bufSize);
      auto offsetLast = maxPage * bufSize;
      ioChannel.pushBlocking(IoRequestType::Read, buf, offsetLast, bufSize);
      const bool check2 = fileState.checkBufferNoThrow(buf, offsetLast, bufSize);
      std::cout << "check " << offsetLast << ": " << check2 << std::endl;
      iniDoneCheck &= check2;
      std::cout << "init check: " << (iniDoneCheck ? "probably already done" : "not done yet") << std::endl;
      if (!autoInit && !iniDoneCheck && !disableCheck) {
         throw std::logic_error("Init not done yet on " + IoInterface::instance().getDeviceInfo().names() + ". If you know what you are doing use INIT=disable or INIT=auto / INIT=yes to force SSD initialization.");
      }
   }
   if (!disableCheck) {
      if (forceInit || !iniDoneCheck) {
         auto start = getSeconds();
         std::cout << ">> init: " << std::endl
                   << std::flush;
         std::cout << "init " << iniOps * initBufSize / 1024 / 1024 / 1024 << " GB iniOps: " << iniOps << std::endl
                   << std::flush;
         JobOptions initOptions;
         initOptions.name = "init";
         initOptions.bs = initBufSize;
         initOptions.filesize = iniOps * initBufSize; // make sure that maxPage * 4kB are at least written
         initOptions.io_size = iniOps * initBufSize;
         initOptions.writePercent = 1;
         initOptions.iodepth = iodepth;
         initOptions.printEverySecond = false;
         atomic<long> time = 0;
         iob::PatternGen::Options pgOptions;
         pgOptions.logicalPages = initOptions.filesize / initOptions.bs;
         pgOptions.pageSize = initOptions.bs;
         pgOptions.patternString = "sequential";
         iob::PatternGen patternGen(pgOptions);
         RequestGenerator init("", initOptions, IoInterface::instance().getIoChannel(0), 0, time, patternGen, fileState);
         init.runIo();
         std::cout << std::endl;
         auto duration = getSeconds() - start;
         std::cout << "init done: " << iniOps * initBufSize / duration / MEBI << "MiB/s ops: " << iniOps << " time: " << duration << std::endl
                   << std::flush;
      }
      // check again
      fileState.resetBufferChecks(buf, bufSize);
      ioChannel.pushBlocking(IoRequestType::Read, buf, 0 * bufSize, bufSize);
      fileState.checkBuffer(buf, 0, bufSize);
      fileState.resetBufferChecks(buf, bufSize);
      ioChannel.pushBlocking(IoRequestType::Read, buf, maxPage * bufSize, bufSize);
      fileState.checkBuffer(buf, maxPage * bufSize, bufSize);
      // check all
   }
   IoInterface::instance().freeIoMemory(buf);
}

class RequestGeneratorThread : public mean::ThreadBase {
 public:
   std::atomic<bool> done = false;
   mean::RequestGenerator gen;
   RequestGeneratorThread(mean::JobOptions jobOptions, int thr, atomic<long>& time, iob::PatternGen& patternGen, mean::FileState& fileState)
       : ThreadBase("gent", thr),
         gen(std::to_string(thr), jobOptions, mean::IoInterface::instance().getIoChannel(thr), thr, time, patternGen, fileState) {
      // setCpuAffinityBeforeStart(thr);
   }
   int process() override {
      gen.runIo();
      done = true;
      return 0;
   }
   bool isDone() const {
      return done;
   }
   void stop() {
      gen.stopIo();
   }
};

std::tuple<mean::JobOptions, mean::IoOptions, iob::PatternGen::Options> loadOptions(int argc, char** argv) {
   CLI::App app{"IO Benchmark Tool"};

   // IO Options
   mean::IoOptions ioOptions;
   app.add_option("--filename", ioOptions.path, "Input file")->envname("FILENAME")->required();
   app.add_option("--ioengine", ioOptions.engine, "IO Engine")->envname("IOENGINE")->default_val("io_uring");
   std::string bsStr = "4K";
   app.add_option("--bs", bsStr, "Block size")->envname("BS")->default_val("4K");
   app.add_option("--iodepth", ioOptions.iodepth, "IO depth")->envname("IO_DEPTH")->default_val(128);
   app.add_flag("--ioupoll", ioOptions.ioUringPollMode, "Enable io_uring poll mode")->envname("IOUPOLL");
   app.add_flag("--ioupt", ioOptions.ioUringNVMePassthrough, "Enable io_uring passthrough")->envname("IOUPT");

   // JobOptions
   mean::JobOptions jobOptions;
   app.add_option("--threads", jobOptions.threads, "Thread count")->envname("THREADS")->default_val(1);
   app.add_option("--runtime", jobOptions.runtimeLimit, "Runtime limit in seconds")->envname("RUNTIME")->default_val(0);
   app.add_option("--rw", jobOptions.writePercent, "Write percentage")->envname("RW")->default_val(0.0);
   app.add_flag("--latencythread", jobOptions.latencyThread, "Enable latency thread")->envname("LATENCYTHREAD");
   std::string filesizeStr;
   app.add_option("--filesize", filesizeStr, "File size")->envname("FILESIZE");
   std::string ioSizeStr;
   double ioSizeTimesFile = 1.0;
   app.add_option("--io_size", ioSizeStr, "IO size")->envname("IO_SIZE");
   app.add_option("--io_size_times_file", ioSizeTimesFile, "IO size as percent of file")->envname("IO_SIZE_PERCENT_FILE");

   app.add_option("--fill", jobOptions.fill, "Fill ratio")->envname("FILL")->default_val(1.0);
   app.add_option("--init", jobOptions.init, "Initialization option")->envname("INIT")->default_val("no");

   app.add_flag("--crc{0}", jobOptions.crc, "Enable CRC checks")->envname("CRC");
   app.add_flag("--random_data{0}", jobOptions.randomData, "Use random data")->envname("RANDOM_DATA");

   app.add_option("--prefix", jobOptions.statsPrefix, "Stats prefix")->envname("PREFIX")->default_val("p");
   app.add_option("--break_every", jobOptions.breakEvery, "Break every N seconds")->envname("BREAK_EVERY")->default_val(0);
   app.add_option("--break_for", jobOptions.breakFor, "Break for N seconds")->envname("BREAK_FOR")->default_val(0);

   app.add_option("--rate", jobOptions.totalRate, "Total IO rate")->envname("RATE")->default_val(0);
   app.add_option("--exprate", jobOptions.exponentialRate, "Use exponential rate")->envname("EXPRATE")->default_val(1);

   app.add_flag("--long_console_output", jobOptions.longConsoleOutput, "Enable long output in console")->envname("LONG_CONSOLE_OUTPUT");

   auto pgOptions = iob::PatternGen::setupCliOptions(app);

   try {
      app.parse(argc, argv);
   } catch (const CLI::ParseError& e) {
      std::exit(app.exit(e));
   }

   jobOptions.filename = ioOptions.path;
   ioOptions.channelCount = jobOptions.threads;

   long bufSize = getBytesFromString(bsStr);
   if (bufSize % 512 != 0) {
      throw std::logic_error("BS is not a multiple of 512");
   }
   if (bufSize % 4096 != 0) {
      std::cout << "BS is not a multiple of 4096. Are you sure that is what you want?" << std::endl;
   }

   mean::IoInterface::initInstance(ioOptions);
   jobOptions.totalfilesize = filesizeStr.empty() ? mean::IoInterface::instance().storageSize() : getBytesFromString(filesizeStr);

   double ioSize = -1;
   if (!ioSizeStr.empty()) {
      ioSize = getBytesFromString(ioSizeStr);
   } else {
      ioSize = jobOptions.totalfilesize * ioSizeTimesFile;
   }
   long opsPerThread = ioSize / bufSize / jobOptions.threads;

   jobOptions.filesize = jobOptions.totalfilesize * jobOptions.fill;
   if ((jobOptions.filesize % (512 * KIBI)) > 0) {
      jobOptions.filesize -= (jobOptions.filesize % (512 * KIBI));
   }
   jobOptions.maxPage = (jobOptions.filesize / bufSize) - 1;

   if (jobOptions.breakEvery > 0) {
      ioSize = -1;
   }

   std::cout << "filename: " << jobOptions.filename;
   std::cout << " filesize: " << jobOptions.totalfilesize;
   std::cout << " use: " << jobOptions.filesize;
   std::cout << " (fill: " << jobOptions.fill << ")";
   std::cout << " in pages: " << jobOptions.maxPage;
   std::cout << std::endl;
   std::cout << "io_size: " << opsPerThread * bufSize / (float)(1 << 30);
   std::cout << " GiB pages: " << opsPerThread << " per thread" << std::endl;
   std::cout << "init: " << jobOptions.init << ", crc: " << jobOptions.crc << ", random: " << jobOptions.randomData << std::endl;
   std::cout << "ionengine: " << ioOptions.engine;
   std::cout << " iodepth: " << ioOptions.iodepth;
   std::cout << " bs: " << bufSize << std::endl;

   if (ioSize == 0) {
      exit(0);
   }

   jobOptions.bs = bufSize;
   jobOptions.iodepth = ioOptions.iodepth;
   jobOptions.io_size = ioSize / jobOptions.threads;
   jobOptions.rateLimit = jobOptions.totalRate / jobOptions.threads;
   jobOptions.disableChecks = jobOptions.init == "disable";

   iob::PatternGen::cliOptionsParsed(*pgOptions, jobOptions.filesize / jobOptions.bs, jobOptions.bs);

   return {jobOptions, ioOptions, *pgOptions};
}

// NOLINTBEGIN(bugprone-exception-escape)
int main(int argc, char** argv) {
   using namespace mean;
   std::cout << "start" << std::endl;
   auto options = loadOptions(argc, argv);
   JobOptions& jobOptions = std::get<0>(options);
   IoOptions& ioOptions = std::get<1>(options);
   iob::PatternGen::Options& pgOptions = std::get<2>(options);

   iob::PatternGen::printPatternHistorgram(pgOptions);
   iob::PatternGen patternGen(pgOptions);

   mean::FileState fileState{(jobOptions.maxPage + 1) * jobOptions.bs, jobOptions.crc, jobOptions.randomData};
   initializeSSDIfNecessary(fileState, jobOptions.maxPage, jobOptions.bs, jobOptions.init, ioOptions.iodepth);

   jobOptions.logHash = getTimeStampStr();
   std::ofstream dump;
   dump.open("iob-dump-" + jobOptions.statsPrefix + ".csv", std::ios_base::app);
   IoTrace::IoStat::dumpIoStatHeader(dump, "iodepth, bs, io_alignment,");
   dump << std::endl;

   std::cout << jobOptions.print();

   string iobLogFilename = "iob-log-" + jobOptions.statsPrefix + ".csv";
   bool iobLogExists = false;
   {
      std::ifstream iobLogExistsFS(iobLogFilename);
      iobLogExists = iobLogExistsFS.good();
   }
   std::ofstream iobLog;
   iobLog.open(iobLogFilename, std::ios_base::app);

   // start threads
   atomic<long> time = 0;
   std::vector<std::unique_ptr<RequestGeneratorThread>> threadVec;
   if (jobOptions.latencyThread) {
      auto propOptions = jobOptions;
      propOptions.name = "gen 0";
      propOptions.rateLimit = 10000;
      propOptions.exponentialRate = false;
      propOptions.writePercent = 0.5;
      threadVec.emplace_back(std::move(std::make_unique<RequestGeneratorThread>(propOptions, 0, time, patternGen, fileState)));
      for (int thr = 1; thr < jobOptions.threads; thr++) {
         jobOptions.name = "gen " + std::to_string(thr);
         jobOptions.rateLimit = (jobOptions.totalRate - ((double)propOptions.rateLimit * propOptions.writePercent)) / (jobOptions.threads - 1);
         threadVec.emplace_back(std::move(std::make_unique<RequestGeneratorThread>(jobOptions, thr, time, patternGen, fileState)));
      }
   } else {
      for (int thr = 0; thr < jobOptions.threads; thr++) {
         jobOptions.name = "gen " + std::to_string(thr);
         jobOptions.rateLimit = jobOptions.totalRate / jobOptions.threads;
         threadVec.emplace_back(std::move(std::make_unique<RequestGeneratorThread>(jobOptions, thr, time, patternGen, fileState)));
      }
   }
   std::this_thread::sleep_for(std::chrono::milliseconds(1));
   for (auto& t: threadVec) {
      t->start();
   }
   long maxRead = 0;
   if (jobOptions.runtimeLimit > 0) {
      std::cout << "runtime: " << jobOptions.runtimeLimit << " s" << std::endl;
   }
   auto start = getSeconds();
   // std::this_thread::sleep_for(std::chrono::seconds(1));
   long lastOCPUpdateTime = -1;
   long prevPhyWrites = -1;
   long prevPhyReads = -1;
   long prevHostWrites = -1;
   while (true) {
      auto now = getSeconds();
      NvmeLog nvmeLog;
      nvmeLog.loadOCPSmartLog();
      long sumReads = 0;
      long sumWrites = 0;
      long sumReadsPS = 0;
      long sumWritesPS = 0;
      /*
      for (int i = 0; i < threads; i++) {
         //IoInterface::instance().getIoChannel(i).printCounters(std::cout);
         //std::cout << std::endl << std::flush;
      }
      */
      for (int i = 0; i < jobOptions.threads; i++) {
         auto& thr = threadVec[i];
         int s = thr->gen.stats.seconds;
         if (s > 0) {
            sumReads += thr->gen.stats.reads;
            sumWrites += thr->gen.stats.writes;
            sumReadsPS += thr->gen.stats.readsPerSecond.at(s - 1);
            sumWritesPS += thr->gen.stats.writesPerSecond.at(s - 1);
            // std::cout << s << " r" << i << ": " << thr->gen.stats.readsPerSecond[s-1]/1000 << "k ";
            // std::cout << s << " w" << i << ": " << thr->gen.stats.writesPerSecond[s-1]/1000 << "k ";
         }
      }
      // std::cout << " total: r: "<< sumRead/1000 << "k " << " w: " << sumWrites << "k " << std::endl << std::flush;
      // maxRead = std::max(maxRead, sumRead);
      if (time == 0) {
         std::stringstream header;
         header << "stat,type,prefix,";
         header << "hash,";
         header << "time,";
         header << "exacttime,";
         header << "device,filesizeGib,fill,usedfilesize,pattern,patternDetails,rate,expRate,writepercent,";
         header << "bs,";
         header << "threads,iodepth,";
         header << "writesTotal,readsTotal,";
         header << "writeIOPS,readIOPS,";
         header << "writesMBs,readsMBs,";
         header << "physicalMediaUnitsWrittenBytes, physicalMediaUnitsReadBytes,";
         header << "phyWriteMBs, phyReadMBs,";
         header << "percentFreeBlocks,";
         header << "softECCError,unalignedIO,maxUserDataEraseCount,minUserDataEraseCount,currentThrottlingStatus,";
         header << "wa";
         if (!iobLogExists) {
            iobLog << header.str() << endl;
            string s = "echo \"prefix,time,hash,type,data\" >> iob-smart-" + jobOptions.statsPrefix + ".csv";
            int sys = system(s.c_str());
            assert(sys == 0);
         }
         if (jobOptions.longConsoleOutput) {
            std::cout << header.str() << std::endl;
         }
      }
      std::stringstream ss;
      ss << "keep,ssd," << jobOptions.statsPrefix;
      ss << "," << jobOptions.logHash;
      ss << "," << time;
      ss << "," << (now - start);
      ss << "," << jobOptions.filename << "," << jobOptions.totalfilesize / GIBI << "," << jobOptions.fill << "," << jobOptions.filesize * 1.0 / GIBI << "," << patternGen.options.patternString << ",\"" << patternGen.patternDetails() << "\"";
      ss << "," << jobOptions.totalRate;
      ss << "," << jobOptions.exponentialRate;
      ss << "," << jobOptions.writePercent;
      ss << "," << jobOptions.bs;
      ss << "," << jobOptions.threads << "," << jobOptions.iodepth;
      ss << "," << sumWrites << "," << sumReads;
      ss << "," << sumWritesPS << "," << sumReadsPS;
      ss << "," << sumWritesPS * jobOptions.bs / MEBI << "," << sumReadsPS * jobOptions.bs / MEBI;
      long currentTotPhyWrites = nvmeLog.physicalMediaUnitsWrittenBytes();
      long currentTotPhyReads = nvmeLog.physicalMediaUnitsReadBytes();
      long thisSecondPhyWrites = 0;
      long thisSecondPhyReads = 0;
      double thisSecondWA = 0;
      if (currentTotPhyWrites != prevPhyWrites) { // ocp was updated, => immediately on kioxia, samsung only every couple of minutes
         if (prevPhyWrites >= 0) {
            thisSecondPhyWrites = (currentTotPhyWrites - prevPhyWrites) / (time - lastOCPUpdateTime);
            thisSecondPhyReads = (currentTotPhyReads - prevPhyReads) / (time - lastOCPUpdateTime);
            thisSecondWA = (currentTotPhyWrites - prevPhyWrites) * 1.0 / ((sumWrites - prevHostWrites) * jobOptions.bs);
         }
         // update prev values
         lastOCPUpdateTime = time;
         prevPhyWrites = currentTotPhyWrites;
         prevPhyReads = currentTotPhyReads;
         prevHostWrites = sumWrites;
      }
      ss << "," << currentTotPhyWrites;
      ss << "," << currentTotPhyReads;
      ss << "," << thisSecondPhyWrites / MEBI;
      ss << "," << thisSecondPhyReads / MEBI;
      ss << "," << (int)nvmeLog.percentFreeBlocks();
      ss << "," << nvmeLog.softECCError() << "," << nvmeLog.unalignedIO() << "," << nvmeLog.maxUserDataEraseCount();
      ss << "," << nvmeLog.minUserDataEraseCount() << "," << (int)nvmeLog.currentThrottlingStatus();
      ss << "," << thisSecondWA;
      iobLog << ss.str() << endl;
      if (jobOptions.longConsoleOutput) {
         std::cout << ss.str() << std::endl;
      } else {
         cout << time;
         cout << " read: " << sumReadsPS*jobOptions.bs/MEBI << " MB/s (" << sumReadsPS << " IOPS)";
         cout << " write: " << sumWritesPS*jobOptions.bs/MEBI << " MB/s (" << sumWritesPS << " IOPS)";
         cout << " WA: " << thisSecondWA << " (phyW: " << thisSecondPhyWrites/MEBI << " phyR: " << thisSecondPhyReads/MEBI << ")";
         cout << std::endl;
      }

      string smartLine = jobOptions.statsPrefix + "," + to_string(time) + "," + jobOptions.logHash + "";
      std::string controller_filename = mean::NvmeLog::extract_nvme_controller(mean::NvmeLog::resolve_symlink(jobOptions.filename));
      string sysSmart =
          "echo -n \"" + smartLine + ",smart,\"                                >> iob-smart-" + jobOptions.statsPrefix + ".csv" +
          " ; sudo nvme smart-log         " + controller_filename + " --output-format=json | tr -d '\\n'      >> iob-smart-" + jobOptions.statsPrefix + ".csv" +
          " ; echo "
          "                                                                          >> iob-smart-" +
          jobOptions.statsPrefix + ".csv";
      string sysOCP =
          "echo -n \"" + smartLine + ",ocp,\"                                 >> iob-smart-" + jobOptions.statsPrefix + ".csv" +
          " ; sudo nvme ocp smart-add-log " + controller_filename + " --output-format=json  2> /dev/null | tr -d '\\n'      >> iob-smart-" + jobOptions.statsPrefix + ".csv" +
          " ; echo "
          "                                                                          >> iob-smart-" +
          jobOptions.statsPrefix + ".csv";
      int sys = system(sysSmart.c_str());
      assert(sys == 0);
      sys = system(sysOCP.c_str());
      assert(sys == 0);
      // cout << sys << endl;

      now = getSeconds();
      bool oneDone = false;
      for (auto& t: threadVec) {
         oneDone |= t->isDone();
      }
      if (oneDone) {
         std::cout << "one done, break;" << std::endl;
         break; // when the first one ist done, stop all
      }
      if (jobOptions.runtimeLimit > 0 && now - start > jobOptions.runtimeLimit) {
         break;
      }
      std::this_thread::sleep_for(std::chrono::microseconds((long)((time + 1 - (now - start)) * 1e6)));
      time++;
   }
   for (auto& t: threadVec) {
      t->stop();
   }

   u64 reads = 0;
   u64 writes = 0;
   double ravg = 0;
   u64 r50p = 0;
   u64 r99p = 0;
   u64 r99p9 = 0;
   u64 w50p = 0;
   double wavg = 0;
   u64 w99p = 0;
   u64 w99p9 = 0;
   u64 rTotalTime = 0;
   u64 wTotalTime = 0;
   double totalTime = 0;
   for (auto& t: threadVec) {
      t->join();
   }
   std::ofstream patDump;
   patDump.open("iob-patdump-" + jobOptions.statsPrefix + ".csv", std::ios_base::app);
   RequestGenerator::dumpPatternAccessHeader(patDump, "");
   std::array<long, 100> accessHist;
   accessHist.fill(0);
   const long sampleCount = 200;
   std::vector<long> sampleLocs(sampleCount);
   std::vector<long> accesses(sampleCount);
   std::random_device randDev;
   std::mt19937_64 mersene{randDev()};
   std::uniform_int_distribution<uint64_t> rndLoc{0, jobOptions.totalMinusOffsetBlocks()};
   for (int i = 0; i < sampleCount; i++) {
      sampleLocs[i] = rndLoc(mersene);
   }
   std::sort(sampleLocs.begin(), sampleLocs.end());
   for (size_t i = 0; i < threadVec.size(); i++) {
      //cout << "thread " << i << endl;
      auto& t = threadVec[i];
      reads += t->gen.stats.reads;
      writes += t->gen.stats.writes;
      totalTime += t->gen.stats.time;
      r50p += t->gen.stats.readHist.getPercentile(50);
      r99p += t->gen.stats.readHist.getPercentile(99);
      r99p9 += t->gen.stats.readHist.getPercentile(99.9);
      w50p += t->gen.stats.writeHist.getPercentile(50);
      w99p += t->gen.stats.writeHist.getPercentile(99);
      w99p9 += t->gen.stats.writeHist.getPercentile(99.9);
      ravg += t->gen.stats.readHist.getAverage();
      wavg += t->gen.stats.writeHist.getAverage();
      rTotalTime += t->gen.stats.readTotalTime;
      wTotalTime += t->gen.stats.writeTotalTime;
      t->gen.ioTrace.dumpIoTrace(dump, std::to_string(jobOptions.iodepth) + "," + std::to_string(jobOptions.bs) + "," + std::to_string(0 /*alignment compatibility*/) + ",");
      t->gen.aggregatePatternAccess(accessHist);
      t->gen.samplePatternAccess(sampleLocs, accesses);
      // t->gen.dumpPatternAccess("patterDump:: ", cout);
      //cout << endl;
   }
   /*
   cout << "sample:" << endl;
   for (auto s: sampleLocs) {
      cout << s << ",";
   }
   cout << endl;
   for (auto a: accesses) {
      cout << a << ",";
   }
   cout << endl;
   cout << "hist:" << endl;
   for (auto a: accessHist) {
      cout << a << ",";
   }
   cout << endl;
   */
   totalTime /= jobOptions.threads;
   ravg /= jobOptions.threads;
   r50p /= jobOptions.threads;
   r99p /= jobOptions.threads;
   r99p9 /= jobOptions.threads;
   w50p /= jobOptions.threads;
   wavg /= jobOptions.threads;
   w99p /= jobOptions.threads;
   w99p9 /= jobOptions.threads;
   dump << "filesize,fill,usedFileSize,io_size,filename,bs,rw,threads,iodepth,reads,writes,rmb,wmb,ravg,wavg,r50p,r99p,r99p9,w50p,w99p,w99p9" << std::endl;
   dump << jobOptions.filesize << "," << jobOptions.fill << "," << jobOptions.filesize << "," << jobOptions.io_size << ",";
   dump << "\"" << jobOptions.filename << "\"," << jobOptions.bs << "," << jobOptions.writePercent << "," << jobOptions.threads << "," << jobOptions.iodepth << ",";
   dump << reads / totalTime << "," << writes / totalTime << "," << reads / totalTime * jobOptions.bs / MEBI << "," << writes / totalTime * jobOptions.bs / MEBI << ",";
   dump << std::setprecision(6) << (float)reads / rTotalTime * 1e6 << "," << (float)writes / wTotalTime * 1e6 << "," << r50p << "," << r99p << "," << r99p9 << "," << w50p << "," << w99p << "," << w99p9 << "," << std::endl;
   dump.close();

   std::cout << "summary ";
   std::cout << std::setprecision(4);
   std::cout << "total: " << (reads + writes) / totalTime / MEGA << " MIOPS";
   std::cout << " (reads: " << reads / totalTime / MEGA << " write: " << writes / totalTime / MEGA << ")";
   std::cout << " total: " << (reads + writes) / totalTime * jobOptions.bs / GIBI << " GiB/s";
   std::cout << " (reads: " << reads / totalTime * jobOptions.bs / GIBI << " write: " << writes / totalTime * jobOptions.bs / GIBI << ")";
   if (maxRead > 0) {
      std::cout << " max read: " << maxRead / 1e6 << "M";
   };
   std::cout << std::endl;
   std::cout << "latency [us]: read avg: " << ravg << " 50p: " << r50p << " 99p: " << r99p << " 99.9p: " << r99p9;
   std::cout << " write avg: " << wavg << " 50p: " << w50p << " 99p: " << w99p << " 99.9p: " << w99p9 << std::endl;

   std::this_thread::sleep_for(std::chrono::seconds(1));
   std::cout << "fin" << std::endl;

   return 0;
}
// NOLINTEND(bugprone-exception-escape)