//
// Created by Gabriel Haas on 02.07.24.
//
#pragma once

#include "Exceptions.hpp"
#include "SSD.hpp"

#include <algorithm>
#include <csignal>
#include <fstream>
#include <list>
#include <random>

class GreedyGC {
   SSD& ssd;
   uint64_t currentBlock;
   int64_t currentGCBlock = -1;
   // k - greedy
   int k;
   bool simpleTwoR;
   std::random_device rd;
   std::mt19937_64 gen{rd()};
   std::uniform_int_distribution<uint64_t> rndBlockDist;
   std::list<uint64_t> freeBlocks;

 public:
   GreedyGC(SSD& ssd, int k = 0, bool twoR = false) : ssd(ssd), k(k), simpleTwoR(twoR), rndBlockDist(0, ssd.blockCount - 1) {
      for (uint64_t z = 0; z < ssd.blockCount; z++) {
         freeBlocks.push_back(z);
      }
      currentBlock = freeBlocks.front();
      freeBlocks.pop_front();
   }
   string name() const {
      if (simpleTwoR) {
         return "greedy-s2r";
      }
      if (k == 0) {
         return "greedy";
      }
      return "greedy-k" + std::to_string(k);
   }
   void writePage(uint64_t pageId) {
      if (!ssd.blocks()[currentBlock].canWrite()) {
         if (freeBlocks.empty()) {
            performGC();
         }
         currentBlock = freeBlocks.front();
         freeBlocks.pop_front();
         ensure(ssd.blocks()[currentBlock].canWrite());
      }
      ssd.writePage(pageId, currentBlock);
   }
   int64_t singleGreedy() {
      int64_t minIdx = -1;
      uint64_t minCnt = std::numeric_limits<uint64_t>::max();
      for (uint64_t i = 0; i < ssd.blockCount; i++) {
         const SSD::Block& block = ssd.blocks()[i];
         if (block.validCnt() < minCnt && block.fullyWritten()) { // only use full blocks for gc
            minIdx = i;
            minCnt = block.validCnt();
         }
      }
      ensure(minIdx != -1);
      return minIdx;
   }
   int64_t singleGreedyLimited() {
      constexpr int n = 10000;
      int64_t minIdx = -1;
      uint64_t minCnt = std::numeric_limits<uint64_t>::max();
      std::uniform_int_distribution<uint64_t> dist(0, ssd.blockCount - 1);
      uint64_t startIdx = dist(gen);
      uint64_t idx = startIdx;
      for (uint64_t i = 0; i < n; i++) {
         const SSD::Block& block = ssd.blocks()[idx];
         if (block.fullyWritten() && block.validCnt() < minCnt) {
            minIdx = idx;
            minCnt = block.validCnt();
         }
         idx++;
         if (idx == ssd.blockCount) { idx = 0; }
      }
      ensure(minIdx != -1);
      return minIdx;
   }
   void performGC() {
      if (!simpleTwoR) {
         int64_t victimBlockIdx = -1;
         if (k == 0) {
            victimBlockIdx = singleGreedy();
            //victimBlockIdx = singleGreedyLimited();
            // writezone = random() % zones;
         } else {
            // perform k-greedy
            uint64_t minIdx = -1;
            uint64_t minCnt = std::numeric_limits<uint64_t>::max();
            int i = 0;
            do {
               uint64_t idx = rndBlockDist(gen);
               if (ssd.blocks().at(idx).validCnt() < minCnt) {
                  minIdx = idx;
                  minCnt = ssd.blocks()[idx].validCnt();
               }
               i++;
            } while (i < k);
            victimBlockIdx = minIdx;
         }
         ensure(victimBlockIdx != -1);
         ssd.compactBlock(victimBlockIdx);
         freeBlocks.push_back(victimBlockIdx);
      } else {
         // compact until free block using singleGreedy
         auto [freeBlock, gcBlock] = ssd.compactUntilFreeBlock(currentGCBlock, [&]() { return singleGreedy(); });
         assert(ssd.blocks()[freeBlock].isErased());
         currentGCBlock = gcBlock;
         freeBlocks.push_back(freeBlock);
      }
   }
   void stats() {
      std::cout << "Greedy stats" << std::endl;
   }
   void resetStats() {}
};
