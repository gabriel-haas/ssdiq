#include "Exceptions.hpp"
#include "IoEnvironment.hpp"

namespace mean {
void* IoEnvironment::allocIoMemoryChecked(size_t size, size_t align) {
   auto* mem = allocIoMemory(size, align);
   null_checkm(mem, "Memory allocation failed");
   return mem;
};
} // namespace mean