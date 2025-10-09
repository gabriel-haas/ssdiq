#include "Env.hpp"
#include "Greedy.hpp"
#include "PatternGen.hpp"
#include "SSD.hpp"
#include "Time.hpp"
#include "TwoR.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <random>
#include <string>

using std::cout;
using std::endl;
using std::string;
using namespace iob;
namespace fs = std::filesystem;

struct SimOptions {
   std::string pageStr;
   std::string capacityStr;
   std::string eraseStr;
   std::string prefix;
   float ssdFill;
   bool initLoad;
   std::string gcAlgorithm;
   bool switchDist;
   int mdcBatch;
   int timestamps;
   int writeHeads;
   int optHistSize;
   float printEverySSDWrite;
};
template <typename GCAlgo>
void runBench(GCAlgo& gc, SSD& ssd, PatternGen::Options& pgOptions, SimOptions& options) {
   std::random_device randDevice;
   std::mt19937_64 rng{randDevice()};
   // cout << "writesPerRep: " << (float)((writesPerRep * pageSize) / (float)gb) << " GB" << endl;
   std::unique_ptr<PatternGen> pg = std::make_unique<PatternGen>(pgOptions);

   // seq init, guarantees ssd is full,
   for (uint64_t i = 0; i < ssd.logicalPages; i++) {
      gc.writePage(i);
   }
   if (options.initLoad) {
      // a batch of writes based on access pattern to fill OP
      // uint64_t writeOP = ssd.physicalPages - ssd.logicalPages;
      uint64_t writeOP = ssd.physicalPages;
      for (uint64_t i = 0; i < writeOP; i++) {
         uint64_t logPage = pg->accessPatternGenerator(rng);
         gc.writePage(logPage);
      }
      cout << "Init WA: " << std::to_string(((float)ssd.physWrites()) / ssd.logicalPages) << endl;
   }
   ssd.resetPhysicalCounters();
   gc.resetStats();

   std::string logHash = mean::getTimeStampStr();
   std::string filename = "sim_" + options.gcAlgorithm + "_" + logHash + "_" + options.gcAlgorithm + "_wh" + std::to_string(options.writeHeads) + "_ts" + std::to_string(options.timestamps) + "_mb" + std::to_string(options.mdcBatch) + "_" + options.prefix + ".csv";
   bool fileExists = std::filesystem::exists(filename);
   std::ofstream logFile(filename, std::ios::app);
   if (!logFile.is_open()) {
      std::cerr << "Error opening runBench log file." << std::endl;
      return;
   }

   std::string header = "sim,hash,prefix,ssdwrites,rep,time,capacity,erase,pagesize,pattern,skew,zones,alpha,beta,ssdFill,gc,";
   header += "mdcbatch,writeheads,timestamps,opthistsize,";
   header += "freePercentaftergc,runningWAF,cumulativeWAF";
   cout << header << endl;
   if (!fileExists) {
      logFile << header << endl;
   }

   // bench
   uint64_t writesPerRep = ssd.logicalPages / options.printEverySSDWrite;
   uint64_t numReps = pg->options.totalWrites / writesPerRep;
   uint64_t cumulativePhysWrites = 0; // Cumulative physical writes across all repetitions
   uint64_t cumulativeLogWrites = 0;  // Cumulative logical writes across all repetitions
   auto start = mean::getSeconds();
   for (uint64_t rep = 0; rep < numReps; rep++) {
      if (options.switchDist && rep == numReps/2) {
         pg = std::make_unique<PatternGen>(pgOptions);
      }

      for (uint64_t i = 0; i < writesPerRep; i++) {
         uint64_t logPage = pg->accessPatternGenerator(rng);
         gc.writePage(logPage);
         cumulativeLogWrites++;
      }

      cumulativePhysWrites += ssd.physWrites();
      float currentWAF = ((float)ssd.physWrites()) / writesPerRep;
      float cumulativeWAF = (float)cumulativePhysWrites / (float)cumulativeLogWrites;
      auto now = mean::getSeconds();
      auto s = std::format("bench,{},'{}',{},{},{:.2f},{},{},{},{},{},'{}',{},{},{:.4f},",
         logHash, options.prefix, (float)rep*1/options.printEverySSDWrite, rep, now - start, ssd.capacityBytes, ssd.blockSizeBytes, ssd.pageSizeBytes,
         pg->options.patternString, pg->options.skewFactor, pg->patternDetails(),
         pg->options.alpha, pg->options.beta, ssd.ssdFill);
      s += std::format("{},{},{},{},{},",
         gc.name(),
         options.mdcBatch, options.writeHeads, options.timestamps, options.optHistSize);
      s += std::format("{:.4f},{:.5f},{:.5f}\n",
         writesPerRep / (float)ssd.physWrites(), currentWAF, cumulativeWAF);
      cout << s << std::flush;
      logFile << s << std::flush;
      ssd.resetPhysicalCounters();
      // ssd.printBlocksStats();
      gc.stats();
   }
   // ssd.printBlocksStats();

   logFile.close();

   // pg.generateAccessFrequencyHistogram(ssd.writtenPages, ssd.ssdFill);
   //  Save the access pattern data to file and generate the plot
}

// NOLINTBEGIN(bugprone-exception-escape)
int main(int argc, char** argv) {
   CLI::App app{"SSD Simulator"};

   SimOptions options;

   app.add_option("--page", options.pageStr, "Page size (e.g., 4K)")->envname("PAGE")->default_val("4K");
   app.add_option("--capacity", options.capacityStr, "SSD capacity (e.g., 16G)")->envname("CAPACITY")->default_val("16G");
   app.add_option("--erase", options.eraseStr, "Block erase size (e.g., 8M)")->envname("ERASE")->default_val("8M");
   app.add_option("--prefix", options.prefix, "Prefix for output/log files")->envname("PREFIX")->default_val("output");
   app.add_option("--ssdfill", options.ssdFill, "SSD fill ratio (0 <= fill <= 1)")->envname("SSDFILL")->check(CLI::Range(0.0F, 1.0F))->default_val("0.875");
   app.add_option("--load", options.initLoad, "Initial load")->envname("LOAD")->default_val(true);
   app.add_option("--gc", options.gcAlgorithm, "GC algorithm")->envname("GC")->default_val("greedy");
   app.add_flag("--switch-dist", options.switchDist, "resets the distribution after half the writes")->envname("SWITCH_DIST")->default_val(false);
   app.add_option("--print-every", options.printEverySSDWrite, "Print every 1/nth SSD writes")->envname("PRINT_EVERY_SSD_WRITE")->default_val(10);
   // gc options
   app.add_option("--mdc-batch", options.mdcBatch, "MDC batch size")->envname("MDC_BATCH")->default_val(64);
   // tt
   app.add_option("--timestamps", options.timestamps, "Number of timestamps")->envname("TIMESTAMPS")->default_val(4);
   // 2a
   app.add_option("--write-heads", options.writeHeads, "Number of write heads")->envname("WRITE_HEADS")->default_val(20);
   // opt
   app.add_option("--opt-hist-size", options.optHistSize, "Optimal GC history size")->envname("OPT_HIST_SIZE")->default_val(1000);

   std::unique_ptr<iob::PatternGen::Options> pgOptions = iob::PatternGen::setupCliOptions(app);

   try {
      app.parse(argc, argv);
   } catch (const CLI::ParseError& e) {
      std::exit(app.exit(e));
   }

   uint64_t pageSize = getBytesFromString(options.pageStr);
   uint64_t capacity = getBytesFromString(options.capacityStr);
   uint64_t blockSize = getBytesFromString(options.eraseStr);
   SSD ssd(capacity, blockSize, pageSize, options.ssdFill);
   ssd.printInfo();

   iob::PatternGen::cliOptionsParsed(*pgOptions, ssd.logicalPages, ssd.pageSizeBytes);
   iob::PatternGen::printPatternHistorgram(*pgOptions);
   // Pattern generation options

   cout << "switch: " << options.switchDist << " load: " << options.initLoad << endl;

   if (options.gcAlgorithm == "greedy") {
      GreedyGC greedy(ssd);
      runBench(greedy, ssd, *pgOptions, options);
   } else if (options.gcAlgorithm.contains("greedy-k")) {
      int k = std::stoi(options.gcAlgorithm.substr(8));
      GreedyGC greedy(ssd, k);
      runBench(greedy, ssd, *pgOptions, options);
   } else if (options.gcAlgorithm.contains("greedy-s2r")) {
      GreedyGC greedy(ssd, 0, true);
      runBench(greedy, ssd, *pgOptions, options);
   } else if (options.gcAlgorithm.contains("2r")) {
      TwoR twoR(ssd, options.gcAlgorithm);
      runBench(twoR, ssd, *pgOptions, options);
   } else if (options.gcAlgorithm.contains("deathtime")) {
      // DTE edt(ssd, gcAlgorithm);
      // runBench(edt, ssd, pg, targetWrites, initLoad);
   } else {
      throw std::runtime_error("unknown gc algorithm: " + options.gcAlgorithm);
   }
   return 0;
}
// NOLINTEND(bugprone-exception-escape)
