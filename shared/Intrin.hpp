#pragma once

#include "Units.hpp"

#ifdef __x86_64__
#include <x86intrin.h>
#endif

namespace intrin {
// -------------------------------------------------------------------------------------
inline uint64_t readTSC() {
#ifdef __x86_64__
   const uint64_t tsc = __rdtsc();
   return tsc;
#else
   return 0;
#endif
}
inline uint64_t readTSCfenced() {
#ifdef __x86_64__
   _mm_mfence();
   const uint64_t tsc = __rdtsc();
   _mm_mfence();
   return tsc;
#else
   return 0;
#endif
}
// -------------------------------------------------------------------------------------
inline void pause() {
#ifdef __x86_64__
   _mm_pause();
#endif
}
// -------------------------------------------------------------------------------------
} // namespace intrin
// -------------------------------------------------------------------------------------
