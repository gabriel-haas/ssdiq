#pragma once

#include "IoEnvironment.hpp"
#include "IoOptions.hpp"
// -------------------------------------------------------------------------------------
namespace mean {
// -------------------------------------------------------------------------------------
class IoInterface {
   static std::unique_ptr<IoEnvironment> _instance;

 public:
   IoInterface() = delete;
   IoInterface(const IoInterface&) = delete;
   static IoEnvironment& initInstance(const IoOptions& ioOptions);
   static IoEnvironment& instance();
   // -------------------------------------------------------------------------------------
   static IoChannel& getIoChannel(int channel);
   static void* allocIoMemoryChecked(size_t size, size_t align);
   static void freeIoMemory(void* ptr, size_t size = 0);
};
// -------------------------------------------------------------------------------------
} // namespace mean
// -------------------------------------------------------------------------------------
