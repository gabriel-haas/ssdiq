#include "IoInterface.hpp"
#include "IoEnvironmentImpl.hpp"
#include <memory>
#include <stdexcept>
// -------------------------------------------------------------------------------------
#include "impl/LibaioImpl.hpp"
#include "impl/LiburingImpl.hpp"
// -------------------------------------------------------------------------------------
namespace mean {
std::unique_ptr<IoEnvironment> IoInterface::_instance = nullptr;
// -------------------------------------------------------------------------------------
IoEnvironment& IoInterface::initInstance(const IoOptions& ioOptions) {
   if (ioOptions.engine == "libaio") {
      _instance = std::unique_ptr<IoEnvironment>(new IoEnvironmentImpl<LibaioEnv, LibaioChannel, LibaioIoRequest>(ioOptions));
#ifdef LEANSTORE_INCLUDE_SPDK
   } else if (ioOptions.engine == "spdk") {
      _instance = std::unique_ptr<RaidEnvironment>(new RaidEnv<SpdkEnv, SpdkChannel, SpdkIoReq>(ioOptions));
#endif
   } else if (ioOptions.engine == "io_uring") {
      _instance = std::unique_ptr<IoEnvironment>(new IoEnvironmentImpl<LiburingEnv, LiburingChannel, LiburingIoRequest>(ioOptions));
#ifdef LEANSTORE_INCLUDE_XNVME
   } else if (ioOptions.engine.find("xnvme") != string::npos) {
      _instance = std::unique_ptr<RaidEnvironment>(new RaidEnv<XnvmeEnv, XnvmeChannel, XnvmeRequest>(ioOptions));
#endif
   } else {
      throw std::logic_error("not implemented");
   }
   return *_instance;
}
IoEnvironment& IoInterface::instance() {
   ensurem(_instance.get(), "IoEnvironment not initialized.");
   return *_instance;
}
IoChannel& IoInterface::getIoChannel(int channel) {
   return instance().getIoChannel(channel);
}
void* IoInterface::allocIoMemoryChecked(size_t size, size_t align) {
   return instance().allocIoMemoryChecked(size, align);
}
void IoInterface::freeIoMemory(void* ptr, size_t size) {
   instance().freeIoMemory(ptr, size);
}
// -------------------------------------------------------------------------------------
} // namespace mean
// -------------------------------------------------------------------------------------
