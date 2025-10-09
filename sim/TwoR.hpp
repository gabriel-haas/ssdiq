#include "SSD.hpp"

#include <algorithm>
#include <list>
#include <random>
#include <set>
#include <vector>

class TwoR {
   uint64_t currentBlock;
   std::list<uint64_t> freeBlocks;
   std::vector<uint64_t> normalBlocks;
   std::vector<uint64_t> coldBlocks;

   // Global FIFO list and pointers for 2R-FIFO
   double BLK_UTIL = 0.5;        // Block utilization threshold for selective merge policy
   double FIFO_SCAN_DEPTH = 0.7; // FIFO scan depth for second chance policy

   std::list<uint64_t> fifoList;
   std::list<uint64_t>::iterator curScan;
   std::list<uint64_t>::iterator curColdBlkPtr;
   std::list<uint64_t>::iterator curBlkPtr;

   // frequency generator to choose region (normal vs. cold)
   std::random_device rd;
   std::default_random_engine generator{rd()};
   std::uniform_int_distribution<int> distribution{0, 100};

   SSD& ssd;
   std::string gcAlgorithm;

 public:
   TwoR(SSD& ssd, std::string gcAlgorithm) : ssd(ssd), gcAlgorithm(gcAlgorithm) {
      // Initialize free block list
      for (unsigned z = 0; z < ssd.blockCount; z++) {
         freeBlocks.push_back(z);
      }
      initializeFIFOList();

      currentBlock = freeBlocks.front();
      freeBlocks.pop_front();
   }

   std::string name() {
      return gcAlgorithm;
   }

   // Function to initialize FIFO list and pointers
   void initializeFIFOList() {
      fifoList.clear();

      curScan = fifoList.begin();
      curColdBlkPtr = fifoList.end();
      curBlkPtr = fifoList.end();
   }

   void writePage(uint64_t pageId) {
      if (!ssd.blocks()[currentBlock].canWrite()) {
         fifoList.push_back(currentBlock);
         normalBlocks.push_back(currentBlock);
         if (freeBlocks.empty()) {
            performGC();
         }
         ensure(!freeBlocks.empty());
         currentBlock = freeBlocks.front();
         freeBlocks.pop_front();
         ensure(ssd.blocks()[currentBlock].canWrite());
      }
      ssd.writePage(pageId, currentBlock);
   }

   void performGC() {
      if (freeBlocks.empty()) {
         if (gcAlgorithm == "2r-fifo") {
            garbageCollect2RFIFO();
         } else if (gcAlgorithm == "2r-greedy") {
            garbageCollect2RGreedy();
         } else {
            throw std::runtime_error("unknown GC algorithm");
         }
      }
   }

   uint64_t greedyNormal() {
      auto it = std::min_element(normalBlocks.begin(), normalBlocks.end(),
                                 [&](uint64_t a, uint64_t b) { return ssd.blocks()[a].validCnt() < ssd.blocks()[b].validCnt(); });
      return (it != normalBlocks.end()) ? *it : ssd.blockCount;
   }

   uint64_t greedyCold() {
      auto it = std::min_element(coldBlocks.begin(), coldBlocks.end(),
                                 [&](uint64_t a, uint64_t b) { return ssd.blocks()[a].validCnt() < ssd.blocks()[b].validCnt(); });
      return (it != coldBlocks.end()) ? *it : ssd.blockCount;
   }

   uint64_t getTotalInvalidPagesCold() {
      uint64_t invalidPagesCnt = 0;
      for (auto& b: coldBlocks) {
         invalidPagesCnt += (ssd.pagesPerBlock - ssd.blocks()[b].validCnt());
      }
      // fprintf(stderr, "coldregion: %lu\n", invalidPagesCnt);
      // cout << "coldregion: " << invalidPagesCnt << endl;
      return invalidPagesCnt;
   }

   uint64_t getTotalInvalidPagesNormal() {
      uint64_t invalidPagesCnt = 0;
      for (auto& b: normalBlocks) {
         invalidPagesCnt += (ssd.pagesPerBlock - ssd.blocks()[b].validCnt());
      }
      // fprintf(stderr, "normalregion: %lu\n", invalidPagesCnt);
      // cout << "normalregion: " << invalidPagesCnt << endl;
      return invalidPagesCnt;
   }

   bool garbageCollectColdRegion(uint64_t physWrites) {
      if (getTotalInvalidPagesCold() < ssd.pagesPerBlock) {
         return false;
      }
      if (getTotalInvalidPagesNormal() < ssd.pagesPerBlock) {
         return true;
      }
      // if(coldBlocks.size()> ssd.zones* 0.7){
      //     // used for zipfian
      //     return true;
      // }
      // Select region to garbage collect based on a random number
      uint32_t minHotValidCnt = ssd.blocks()[greedyNormal()].validCnt();
      uint32_t minColdValidCnt = ssd.blocks()[greedyCold()].validCnt();

      // uint32_t minValidCnt = std::min(minHotValidCnt, minColdValidCnt);

      if (minHotValidCnt < minColdValidCnt) {
         return false;
      } else {
         return true;
      }
   }

   void selectVictimBlocksGreedy(std::vector<uint64_t>& victimIds, uint64_t* totalInvalidPages) {
      std::set<uint64_t> selectedVictims; // To keep track of already selected victim blocks
      // collect from cold region
      if (garbageCollectColdRegion(ssd.physWrites())) {
         while (*totalInvalidPages < ssd.pagesPerBlock && !coldBlocks.empty()) {
            uint64_t victimId = 0;
            do {
               victimId = greedyCold();
            } while (selectedVictims.find(victimId) != selectedVictims.end());

            victimIds.push_back(victimId);
            coldBlocks.erase(std::remove(coldBlocks.begin(), coldBlocks.end(), victimId), coldBlocks.end()); // Remove from coldBlocks
            selectedVictims.insert(victimId);                                                                // Mark this block as selected
            *totalInvalidPages += (ssd.pagesPerBlock - ssd.blocks()[victimId].validCnt());
         }
      } else {
         // collect form normal region
         while (*totalInvalidPages < ssd.pagesPerBlock && !normalBlocks.empty()) {
            uint64_t victimId = 0;
            // Find the next block using greedy that is not already selected
            do {
               victimId = greedyNormal();
            } while (selectedVictims.find(victimId) != selectedVictims.end());

            victimIds.push_back(victimId);
            normalBlocks.erase(std::remove(normalBlocks.begin(), normalBlocks.end(), victimId), normalBlocks.end()); // Remove from normalBlocks
            selectedVictims.insert(victimId);                                                                        // Mark this block as selected
            *totalInvalidPages += (ssd.pagesPerBlock - ssd.blocks()[victimId].validCnt());
         }
      }
      ensure(*totalInvalidPages >= ssd.pagesPerBlock);
   }

   void garbageCollect2RGreedy() {
      std::vector<uint64_t> victimIds;
      uint64_t totalInvalidPages = 0;

      // Select victim blocks
      selectVictimBlocksGreedy(victimIds, &totalInvalidPages);
      // /cout << victimIds.size() << " " << totalInvalidPages << endl;

      // Compact victim blocks
      [[maybe_unused]] uint64_t lastBlock = ssd.compactUntilFreeBlock(victimIds);

      // Move remaining victim blocks to freeBlocks
      for (uint64_t victimId: victimIds) {
         if (!ssd.blocks()[victimId].canWrite()) {
            // cout << "all full: " << victimId << endl;
            coldBlocks.push_back(victimId);
         } else {
            ensure(ssd.blocks()[victimId].canWrite());
            // cout << "canwrite: " << victimId << endl;
            freeBlocks.push_back(victimId);
         }
      }
      ensure(freeBlocks.size());
      victimIds.clear();
   }

   void selectVictimBlocksFIFO(std::vector<uint64_t>& victimIds) {
      uint64_t totalInvalidPages = 0;
      static size_t stepsSinceReset = 0; // keep track of how far we've gone
      if (curScan == fifoList.end()) {
         curScan = fifoList.begin();
         stepsSinceReset = 0; // reset counter if we wrap
      }
      const size_t threshold = static_cast<size_t>(FIFO_SCAN_DEPTH * fifoList.size());
      while (totalInvalidPages < ssd.pagesPerBlock && curScan != fifoList.end()) {
         uint64_t victimId = *curScan;
         // Check if victimId is already in victimIds
         if (std::find(victimIds.begin(), victimIds.end(), victimId) == victimIds.end()) {
            // Select block only if its utilization is below the threshold
            if (ssd.blocks()[victimId].validCnt() / double(ssd.pagesPerBlock) < BLK_UTIL) {
               victimIds.push_back(victimId);
               totalInvalidPages += (ssd.pagesPerBlock - ssd.blocks()[victimId].validCnt());
            }
         }
         ++curScan;
         ++stepsSinceReset;
         // second chance policy
         if (stepsSinceReset >= threshold) {
            curScan = fifoList.begin();
            stepsSinceReset = 0;
            BLK_UTIL += 0.05;
         }
      }
      BLK_UTIL = 0.7;
      if (totalInvalidPages < ssd.pagesPerBlock) {
         std::cout << "cur victim blocks not enough" << std::endl;
      }
   }

   void garbageCollect2RFIFO() {
      std::vector<uint64_t> victimIds;
      victimIds.clear();

      // Select victim blocks
      selectVictimBlocksFIFO(victimIds);

      // Compact victim blocks
      [[maybe_unused]] uint64_t lastBlock = ssd.compactUntilFreeBlock(victimIds);

      // Move remaining victim blocks to freeBlocks
      for (uint64_t victimId: victimIds) {
         if (!ssd.blocks()[victimId].canWrite()) {
            coldBlocks.push_back(victimId);
            // Insert after curColdBlkPtr and update curColdBlkPtr to point to the position after the inserted element
            if (curColdBlkPtr == fifoList.end()) {
               curColdBlkPtr = fifoList.insert(curColdBlkPtr, victimId);
            } else {
               curColdBlkPtr = fifoList.insert(std::next(curColdBlkPtr), victimId);
            }

            // Move to the next position
            if (curColdBlkPtr != fifoList.end()) {
               ++curColdBlkPtr;
            }

         } else {
            ensure(ssd.blocks()[victimId].canWrite());
            freeBlocks.push_back(victimId);
         }

         // Ensure curScan points to a valid position after erase
         auto it = std::find(fifoList.begin(), fifoList.end(), victimId);
         if (it != fifoList.end()) {
            if (curScan == it) {
               curScan = fifoList.erase(it);
            } else {
               fifoList.erase(it);
            }
         }
      }
      // Ensure valid freeBlocks and victimIds are clear
      ensure(freeBlocks.size());
      victimIds.clear();
   }
   void stats() {
      std::cout << "Greedy stats" << std::endl;
   }
   void resetStats() {}
};
