#pragma once

#include <xxhash.h>

#include <cassert>
#include <csignal>
#include <cstdint>
#include <random>
#include <stdexcept>

namespace mean {

constexpr int PAGE_SIZE = 4 * 1024;
constexpr int PAGE_DATA_64BIT = (PAGE_SIZE - 4 * sizeof(uint64_t)) / sizeof(uint64_t);

struct Page {
   int64_t id;
   int64_t version;
   uint64_t data[PAGE_DATA_64BIT];
   int64_t end_id;
   uint64_t checksum;
   void resetChecks() {
      id = -1;
      version = -1;
      end_id = -1;
      checksum = -1;
   }
   uint32_t calcChecksum() const {
      return XXH32(reinterpret_cast<const void*>(this), PAGE_SIZE - sizeof(Page::checksum), 0);
   }
};
static_assert(sizeof(Page) == PAGE_SIZE);

class FileState {
   bool crc;
   bool randomData;

 public:
   FileState(int64_t fileSize, bool crc, bool randomData) : crc(crc), randomData(randomData) {
   }
   ~FileState() = default;
   // set checks on all 4KB pages
   void writeBufferChecks(char* buf, uint64_t offset, uint64_t len, std::mt19937_64& rand) const {
      assert(len % PAGE_SIZE == 0);
      assert((uint64_t)buf % PAGE_SIZE == 0);
      uint64_t pages4k = len / PAGE_SIZE;
      for (uint64_t i = 0; i < pages4k; i++) {
         const uint64_t pageOffset = i * PAGE_SIZE;
         Page* page = reinterpret_cast<Page*>(&buf[pageOffset]);
         uint64_t pageId = (offset + pageOffset) / PAGE_SIZE;
         page->id = pageId;
         page->version = 5;
         page->end_id = page->id;
         page->checksum = 0;
         if (randomData) {
            for (uint64_t& d: page->data) {
               d = rand();
            }
         }
         if (crc) { // last step, calculate crc
            page->checksum = page->calcChecksum();
         }
         // checkBuffer((char*)page, offset+pageOffset, PAGE_SIZE, true);
      }
      // checkBuffer(buf, offset, len, true);
   }
   // check all 4KBs pages
   bool checkBufferNoThrow(char* buf, uint64_t offset, uint64_t len, bool debugDontUnlock = false) const {
      assert(len % PAGE_SIZE == 0);
      assert((uint64_t)buf % PAGE_SIZE == 0);
      uint64_t pages4k = len / PAGE_SIZE;
      for (uint64_t i = 0; i < pages4k; i++) {
         const uint64_t pageOffset = i * PAGE_SIZE;
         Page* page = reinterpret_cast<Page*>(&buf[pageOffset]);
         int64_t pageId = (offset + pageOffset) / PAGE_SIZE;
         bool ok = true;
         ok &= page->id == pageId;
         ok &= page->version == 5;
         ok &= page->end_id == pageId;
         if (crc) {
            ok &= page->checksum == page->calcChecksum();
            // std::cout << page->checksum << std::endl;
         }
         if (!ok) {
            return false;
         }
      }
      return true;
   }
   void checkBuffer(char* buf, uint64_t offset, uint64_t len, bool debugDontUnlock = false) const {
      if (!checkBufferNoThrow(buf, offset, len, debugDontUnlock)) {
         throw std::logic_error("data check failed. Try force initialization with INIT=1");
      }
   }
   void resetBufferChecks(char* buf, uint64_t len) {
      Page* page = reinterpret_cast<Page*>(buf);
      page->resetChecks();
   }
};
} // namespace mean
