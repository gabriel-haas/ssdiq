#pragma once
#include "DeviceInformation.hpp"
#include "IoChannel.hpp"
namespace mean {
class IoEnvironment {
 public:
   virtual ~IoEnvironment() = default;
   // Channel
   virtual IoChannel& getIoChannel(int channel) = 0;
   virtual void freeIoMemory(void* ptr, size_t size = 0) = 0;
   void* allocIoMemoryChecked(size_t size, size_t align);
   // Device
   virtual u64 storageSize() = 0;
   virtual DeviceInformation getDeviceInfo() = 0;

 protected:
   virtual void* allocIoMemory(size_t size, size_t align) = 0;
};
} // namespace mean