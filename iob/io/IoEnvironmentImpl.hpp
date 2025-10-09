#pragma once
#include "DeviceInformation.hpp"
#include "Exceptions.hpp"
#include "IoChannel.hpp"
#include "IoEnvironment.hpp"
#include "IoOptions.hpp"
#include "Raid0Channel.hpp"
namespace mean {
template <typename TIoEnvironment, typename TIoChannel, typename TImplRequest>
class IoEnvironmentImpl : public IoEnvironment {
   std::unique_ptr<TIoEnvironment> io_env;
   std::vector<std::unique_ptr<IoChannel>> channels;
   IoOptions io_options;

 public:
   IoEnvironmentImpl(IoOptions options) : io_options(options) {
      io_env = std::unique_ptr<TIoEnvironment>(new TIoEnvironment());
      io_env->init(options);
      channels.resize(options.channelCount);
      std::cout << "used channels: " << channels.size() << std::endl;
      for (int i = 0; i < options.channelCount; i++) {
         auto& ch = channels[i];
         ch = std::unique_ptr<IoChannel>(new Raid0Channel<TIoEnvironment, TIoChannel, TImplRequest>(*io_env, io_env->getIoChannel(i), io_options, i, io_options.channelCount));
      }
   };
   // Channel
   IoChannel& getIoChannel(int channel) override {
      return *channels.at(channel);
   };
   // Memory
   void freeIoMemory(void* ptr, size_t size = 0) override {
      io_env->freeIoMemory(ptr, size);
   };
   // Device
   u64 storageSize() override {
      return io_env->storageSize();
   };
   DeviceInformation getDeviceInfo() override {
      return io_env->getDeviceInfo();
   };

 protected:
   void* allocIoMemory(size_t size, size_t align) override {
      return io_env->allocIoMemory(size, align);
   };
};
// -------------------------------------------------------------------------------------
} // namespace mean