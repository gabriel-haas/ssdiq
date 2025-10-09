//
// Created by Gabriel Haas on 02.07.24.
//
#pragma once

#include "../shared/Exceptions.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>
// #include <format>

// create types for PageId, BlockId, BlockPos
using PID = uint64_t;
using PHY = uint64_t;
using BID = uint64_t;
using BPOS = uint64_t;
using GID = int64_t;

class SSD {
 public:
   constexpr static uint64_t unused = ~0ULL;
   // incache is an alternative state to unused, that might be necessary for some algorithms, like MDC
   constexpr static uint64_t incache = unused - 1;
   const double ssdFill;    // 1-alpha [0-1]
   const uint64_t capacityBytes; // bytes
   const uint64_t blockSizeBytes; // bytes
   const uint64_t pageSizeBytes; // bytes
   const uint64_t blockCount;
   const uint64_t pagesPerBlock;
   const uint64_t logicalPages;
   const uint64_t physicalPages;

   /* stats */
   std::vector<uint64_t> writtenPages;

   const uint64_t writeBufferSize;
   std::list<PID> writeBuffer; // LRU cache implemented with a list
   uint64_t minFreq = 1;
   static constexpr double writeBufferSizePct = 0; // 0.0002;
   std::unordered_map<PID, std::list<PID>::iterator> writeBufferMap; // Map to quickly find pages in the buffer
   class Block {
      uint64_t _writePos = 0;
      uint64_t _validCnt = 0;
      uint64_t _eraseCount = 0;
      inline static uint64_t _eraseAgeCounter = 0;
      std::vector<PID> _ptl; // phy to log
    public:
      const std::vector<PID>& ptl() const { return _ptl; }
      uint64_t validCnt() const { return _validCnt; }
      uint64_t invalidCnt() const { return pagesPerBlock - _validCnt; }
      uint64_t writePos() const { return _writePos; }
      const uint64_t pagesPerBlock;
      const BID blockId;
      std::vector<uint64_t> invalidationTimes;
      int64_t gcAge = -1;
      int64_t gcGeneration = 0;
      int64_t group = -1;
      bool writtenByGc = false;
      Block(uint64_t pagesPerBlock, BID blockId) : _ptl(pagesPerBlock), pagesPerBlock(pagesPerBlock), blockId(blockId) {
         std::ranges::fill(_ptl, unused);
      }
      uint64_t write(PID logPageId) {
         ensure(canWrite());
         ensure(_ptl[_writePos] == unused);
         _ptl[_writePos] = logPageId;
         _validCnt++;
         return _writePos++;
      }
      void setUnused(BPOS pos) {
         ensure(_ptl[pos] != unused);
         _ptl[pos] = unused;
         _validCnt--;
      }
      bool fullyWritten() const {
         return _writePos == pagesPerBlock;
      }
      bool canWrite() const {
         return _writePos < pagesPerBlock;
      }
      bool allValid() const {
         return _validCnt == pagesPerBlock;
      }
      bool allInvalid() const {
         return _validCnt == 0;
      }
      bool isGCable() const {
         return fullyWritten() && !allValid();
      }
      bool isErased() const {
         return _writePos == 0;
      }
      void compactNoMappingUpdate() {
         _writePos = 0;
         // unlike a real gc that actually moves valid pages to a clean zone
         // before erasing, we just move pages to the beginning of gced zone
         for (BPOS p = 0; p < pagesPerBlock; p++) {
            const uint64_t logpagemove = _ptl[p];
            if (logpagemove != unused) {
               // move page to beginning of zone (might overwrite itself)
               _ptl[_writePos] = logpagemove;
               _writePos++;
            }
         }
         _validCnt = _writePos;
         fill(_ptl.begin() + _writePos, _ptl.end(), unused);
         // this counts as erase
         _eraseCount++;
         gcAge = _eraseAgeCounter++;
         writtenByGc = true;
      }
      void erase() {
         // careful, compact is also an erase
         std::ranges::fill(_ptl, unused);
         _writePos = 0;
         _eraseCount++;
         gcAge = _eraseAgeCounter++;
         writtenByGc = false; // reset
         _validCnt = 0;
         group = -1;
      }
      void print() const {
         std::cout << "age: " << gcAge << " gcGen: " << gcGeneration << " wbgc: " << writtenByGc << " vc: " << _validCnt;
      }
   };

 private:
   std::vector<Block> _blocks;          // phys -> logPageId // FIXME rename blocks
   std::vector<PHY> _ltpMapping;        // logPageId -> physAddr
   std::vector<uint64_t> _mappingUpdatedCnt; // stats
   std::vector<uint64_t> _mappingUpdatedGC;  // stats
   uint64_t _physWrites = 0;
   // stats
   uint64_t gcedNormalBlock = 0;
   uint64_t gcedColdBlock = 0;

 public:
   // const Block& blocks(uint64_t idx) const { return _blocks.at(idx); } // only give read only access
   const decltype(_blocks)& blocks() const { return _blocks; } // only give read only access
   const Block& blocks(uint64_t idx) const { return _blocks.at(idx); } // only give read only access
   const decltype(_ltpMapping)& ltpMapping() const { return _ltpMapping; }
   const decltype(_mappingUpdatedCnt)& mappingUpdatedCnt() const { return _mappingUpdatedCnt; }
   const decltype(_mappingUpdatedGC)& mappingUpdatedGC() const { return _mappingUpdatedGC; }
   uint64_t physWrites() const { return _physWrites; }
   void hackForOptimalWASetPhysWrites(uint64_t phyWrites) { _physWrites = phyWrites; }
   SSD(uint64_t capacityBytes, uint64_t blockSizeBytes, uint64_t pageSizeBytes, double ssdFill)
       : ssdFill(ssdFill), capacityBytes(capacityBytes), blockSizeBytes(blockSizeBytes), pageSizeBytes(pageSizeBytes),
         blockCount(capacityBytes / blockSizeBytes), pagesPerBlock(blockSizeBytes / pageSizeBytes), logicalPages((capacityBytes / pageSizeBytes) * ssdFill), physicalPages(blockCount * pagesPerBlock),
         writeBufferSize(static_cast<uint64_t>(logicalPages * writeBufferSizePct)) {
      // init by sequentially filling blocks
      _ltpMapping.resize(logicalPages);
      _mappingUpdatedCnt.resize(logicalPages);
      _mappingUpdatedGC.resize(logicalPages);
      std::ranges::fill(_ltpMapping, unused);
      for (unsigned z = 0; z < blockCount; z++) {
         _blocks.emplace_back(Block(pagesPerBlock, z));
      }
   }

   BID getBlockId(PHY physAddr) const { return physAddr / pagesPerBlock; }
   BPOS getPagePos(PHY physAddr) const { return physAddr % pagesPerBlock; }
   PHY getPhyAddr(BID blockId, BPOS pos) const { return (blockId * pagesPerBlock) + pos; }

   void writePage(PID logPage, BID block, int64_t group = -1) {
      writePage(logPage, _blocks[block], group);
   }

   void writePage(PID logPage, Block& block, int64_t group = -1) {
      if (writeBufferSize == 0) {
         writePageWithoutCaching(logPage, block, group);
      } else {
         if (writeBufferMap.find(logPage) != writeBufferMap.end()) {
            // Buffer hit, move it to the front (most recently used)
            writeBuffer.erase(writeBufferMap[logPage]);
            writeBuffer.push_front(logPage);
            writeBufferMap[logPage] = writeBuffer.begin();
         } else {
            // Buffer miss
            // push the logPage infront of the LRU list
            writeBuffer.push_front(logPage);
            writeBufferMap[logPage] = writeBuffer.begin();
            if (writeBuffer.size() >= writeBufferSize) {
               // Buffer is full, evict the least recently used page
               uint64_t lruPage = writeBuffer.back();
               writeBufferMap.erase(lruPage);
               writeBuffer.pop_back();
               writePageWithoutCaching(lruPage, block, group);
            }
         }
      }
   }

   // only use from GCup
   void writePageWithoutCaching(PID logPage, Block& block, int64_t group = -1) {
      if (block.group == -1) {
         // std::cout << "set group: " << group << std::endl;
         block.group = group;
      }
      uint64_t addr = _ltpMapping.at(logPage);
      if (addr != unused && addr != incache) { // page is updated, not new
         uint64_t z = getBlockId(addr);
         uint64_t p = getPagePos(addr);
         ensure(z < _blocks.size());
         Block& b = _blocks.at(z);
         b.setUnused(p);
      }
      uint64_t writePos = block.write(logPage);
      _ltpMapping[logPage] = getPhyAddr(block.blockId, writePos);
      _mappingUpdatedCnt[logPage]++;
      _physWrites++;
      // writtenPages.push_back(logPage);
   }

   void setLtpMappingStateCached(PID pid) {
      _ltpMapping[pid] = SSD::incache;
   }

   void eraseBlock(Block& block) {
      block.erase();
   }

   void eraseBlock(BID blockId) {
      ensure(blockId < _blocks.size());
      _blocks[blockId].erase();
      ensure(_blocks[blockId].isErased());
   }

   void compactBlock(uint64_t block) {
      compactBlock(_blocks[block]);
   }
   void compactBlock(Block& block) { // compacts a block by moving active data to the front ~ erase
      block.compactNoMappingUpdate();
      block.gcGeneration++;
      if (block.writtenByGc) {
         gcedColdBlock++;
      } else {
         gcedNormalBlock++;
      }
      block.writtenByGc = true;
      // update mapping for all pages in block
      for (BPOS p = 0; p < block.writePos(); p++) {
         PID logPage = block.ptl()[p];
         ensure(block.ptl()[p] != unused);
         _ltpMapping[logPage] = getPhyAddr(block.blockId, p);
         _mappingUpdatedGC[logPage]++;
         _physWrites++;
      }
   }

   // tries to move valid pages to destination block, set moved pages from source to invalid
   // does not erase source
   bool moveValidPagesTo(BID sourceId, BID destinationId) {
      Block& source = _blocks[sourceId];
      Block& destination = _blocks[destinationId];
      // ensure(!source.allValid());
      if (source.writtenByGc) {
         gcedColdBlock++;
      } else {
         gcedNormalBlock++;
      }
      destination.writtenByGc = true;
      BPOS p = 0;
      while (p < pagesPerBlock && destination.canWrite()) {
         if (source.ptl()[p] != unused) {
            writePageWithoutCaching(source.ptl()[p], destination);
         }
         p++;
      }
      return !source.allInvalid();
   }

   int64_t moveValidPagesTo(BID sourceId, std::function<std::tuple<BID, GID>(PID)> destinationFun) {
      Block& source = _blocks[sourceId];
      ensure(!source.allValid());
      if (source.writtenByGc) {
         gcedColdBlock++;
      } else {
         gcedNormalBlock++;
      }
      BPOS p = 0;
      int64_t firstFullDestinationId = -1;
      while (p < pagesPerBlock) {
         PID lba = source.ptl()[p];
         if (lba != unused) {
            auto [destinationId, groupId] = destinationFun(lba);
            Block& destination = _blocks[destinationId];
            // std::cout << "moveValidPageTo: dest: " << destinationId << std::endl;
            if (destination.canWrite()) { // skip full destinations
               writePageWithoutCaching(source.ptl()[p], destination, groupId);
            } else if (firstFullDestinationId == -1) {
               // std::cout << "moveValidPageTo: first dest full: " << destinationId << std::endl;
               firstFullDestinationId = destinationId;
            }
         }
         p++;
      }
      if (source.allInvalid()) {
         return -1;
      }
      Block& dest = _blocks[firstFullDestinationId];
      ensure(!dest.canWrite());
      return firstFullDestinationId;
   }

   // compacts blocks until a block is completely free
   // returns the free block and the last (not-full) gc block
   std::tuple<BID, BID> compactUntilFreeBlock(BID gcBlockId, std::function<BID()> nextBlock) {
      if (gcBlockId == -1 || _blocks[gcBlockId].allValid()) {
         gcBlockId = nextBlock();
         Block& gcBlock = _blocks[gcBlockId];
         compactBlock(gcBlock);
         ensure(!gcBlock.allValid());
      }
      ensure(!_blocks[gcBlockId].allValid());
      BID victimId = nextBlock();
      while (moveValidPagesTo(victimId, gcBlockId)) {
         // not enough space in gcBlock, compact victimBlock and make it new gcBlock
         Block& victim = _blocks[victimId];
         // Block& gcBlock = _blocks[gcBlockId];
         compactBlock(victim);
         gcBlockId = victimId;
         victimId = nextBlock();
      }
      Block& nowFree = _blocks[victimId];
      nowFree.erase();
      nowFree.gcGeneration = 0;
      return std::make_tuple(victimId, gcBlockId);
   }

   std::tuple<BID, int64_t> compactUntilFreeBlock(GID groupId,
                                                       std::function<BID(GID)> nextBlock,
                                                       std::function<std::tuple<BID, GID>(PID)> gcDestinationFun,
                                                       std::function<void(GID, BID)> updateGroupFun) {
      BID victimId = nextBlock(groupId);
      int64_t fullDest;
      do {
         // std::cout << "gc group: " << groupId << " victim: " << victimId << std::endl;
         fullDest = moveValidPagesTo(victimId, gcDestinationFun);
         if (fullDest == -1) {
            break; // victim is empty
         }
         // not enough space in destination, victim not empty, make victim new destination
         Block& dest = _blocks[fullDest];
         ensure(!dest.canWrite());
         Block& victim = _blocks[victimId];
         ensure(victim.group == groupId);
         ensure(fullDest != victimId);
         //ensure(dest.group == groupId); // this does not hold right now. As gcDestinationFun might write the page to a different frequency group.
         // an alternative would be to have a free list of empty blocks for this case.
         compactBlock(victim);
         ensure(victim.group == groupId);
         // std::cout << "compactUntil: full dest: " << fullDest << " wp: " << dest.writePos() << " dest.group: " << dest.group;
         // std::cout << " victim: " << victimId << " victim.wp: " << victim.writePos()<< std::endl;
         victim.group = dest.group;
         ensure(dest.group != -1);
         ensure(!dest.canWrite());
         updateGroupFun(dest.group, victimId);
         victimId = nextBlock(groupId);
         ensure(_blocks[victimId].group == groupId);
      } while (true);
      Block& nowFree = _blocks[victimId];
      nowFree.erase();
      nowFree.gcGeneration = 0;
      return std::make_tuple(victimId, -1);
   }

   // compacts blocks until a block is completely free
   // returns the final GC block ID that has been erased
   BID compactUntilFreeBlock(std::vector<BID> victimBlockList) {
      // Ensure that the victimBlockList is not empty
      if (victimBlockList.empty()) {
         throw std::invalid_argument("victimBlockList must contain at least one block ID.");
      }
      // If there's only one block, check if it contains any valid pages
      if (victimBlockList.size() == 1) {
         BID singleBlockId = victimBlockList[0];
         ensure(_blocks[singleBlockId].allInvalid());
         eraseBlock(singleBlockId);
         ensure(_blocks[singleBlockId].isErased());
         return singleBlockId;
      }
      // Compact and move valid pages from each block to the previous one
      for (size_t i = victimBlockList.size() - 1; i > 0; i--) {
         BID destBlockId = victimBlockList[i];
         compactBlock(_blocks[destBlockId]);
         BID sourceBlockId = victimBlockList[i - 1];
         moveValidPagesTo(sourceBlockId, destBlockId);
      }
      // Final compact operation on the first block in the list
      BID finalBlockId = victimBlockList[0];
      ensure(_blocks[finalBlockId].allInvalid());
      eraseBlock(finalBlockId);
      ensure(_blocks[finalBlockId].isErased());
      return finalBlockId;
   }

   void resetPhysicalCounters() {
      _physWrites = 0;
   }

   void printInfo() const {
      cout << "capacity: " << capacityBytes << " blocksize: " << blockSizeBytes << " pageSize: " << pageSizeBytes << endl;
      cout << "blockCnt: " << blockCount << " pagesPerBlock: " << pagesPerBlock << " logicalPages: " << logicalPages << " ssdfill: " << ssdFill << endl;
   }

   void stats() {
      auto writtenByGc = std::count_if(_blocks.begin(), _blocks.end(), [](Block& b) { return b.writtenByGc; });
      int64_t maxGCAge = 20;
      std::vector<uint64_t> gcGenerations(maxGCAge, 0);
      std::vector<uint64_t> gcGenerationValid(maxGCAge, 0);
      std::vector<uint64_t> gcGenerationValidMin(maxGCAge, std::numeric_limits<uint64_t>::max());
      // count gc generations
      for (auto& b: _blocks) {
         auto idx = std::min(b.gcGeneration, maxGCAge - 1);
         gcGenerations[idx]++;
         gcGenerationValid[idx] += b.validCnt();
         if (b.fullyWritten()) {
            if (b.validCnt() > pagesPerBlock)
               raise(SIGINT);
            gcGenerationValidMin[idx] = std::min(gcGenerationValidMin[idx], b.validCnt());
         }
      }
      cout << "writtenByGC: " << writtenByGc << " (" << std::round((float)writtenByGc / blockCount * 100) << "%)" << " gcedNormal: " << gcedNormalBlock << " gcedCold: " << gcedColdBlock << endl;
      // print gc generations vector
      cout << "gc generations: [";
      for (int i = 0; i < maxGCAge; i++) {
         // cout << i << ": (c: " << std::format("{:.2f}", gcGenerations[i]*100.0/zones);
         cout << i << ":(c: " << gcGenerations[i] * 100 / blockCount;
         if (gcGenerations[i] > 0) {
            cout << ", f: " << std::round(gcGenerationValid[i] * 100 / gcGenerations[i] / pagesPerBlock);
         } else {
            cout << ", f: 0";
         }
         if (gcGenerationValidMin[i] != std::numeric_limits<uint64_t>::max()) {
            if (gcGenerationValidMin[i] > pagesPerBlock) {
               raise(SIGINT);
            }
            cout << ", m: " << gcGenerationValidMin[i] * 100 / pagesPerBlock;
         } else {
            cout << ", m: x";
         }
         cout << "), ";
      }
      cout << "]" << endl;
      gcedNormalBlock = 0;
      gcedColdBlock = 0;
   }

   void printBlocksStats() {
      cout << "BlockStats: " << endl;
      std::vector<long> ages;
      for (auto& b: _blocks) {
         ages.emplace_back(b.gcAge);
      }
      std::ranges::sort(ages);
      long min = *std::ranges::min_element(ages);
      cout << "age: ";
      for (auto& a: ages) {
         cout << (a - min) << " ";
      }
      cout << endl;
      cout << "gcGen: ";
      for (auto& b: _blocks) {
         cout << b.gcGeneration << " ";
      }
      cout << endl;
      cout << "writtenByGC: ";
      for (auto& b: _blocks) {
         cout << b.writtenByGc << " ";
      }
      cout << endl;
      cout << "ValidCnt: ";
      for (auto& b: _blocks) {
         cout << pagesPerBlock - b.validCnt() << " ";
      }
      cout << endl;
      cout << "Groups: ";
      for (auto& b: _blocks) {
         cout << b.group << " ";
      }
      cout << endl;
   }

   void writeStatsFile(std::string prefix) {
      /*
      auto writeToFile = [&](string filename, std::vector<uint64_t>& vec){
         cout << "write: " << filename << endl;
         std::ofstream myfile;
         myfile.open (filename);
         myfile << "id" << "," << "cnt"<< "\n";
         string s;
         for (uint64_t i = 0; i < vec.size(); i++) {
            if (vec[i] > 0) {
               s = std::to_string(i) + "," + std::to_string(vec[i]) + "\n";
               myfile.write(s.c_str(), s.size());
            }
         }
         myfile.close();
      };
      writeToFile(prefix + "zonedist.csv", _blocks, [&](uint64_t id) { return _blocks[id].validCnt(); });
      writeToFile(prefix + "updates.csv", _mappingUpdatedCnt);
      writeToFile(prefix + "updatesgc.csv", _mappingUpdatedGC);
       */
   }
};
