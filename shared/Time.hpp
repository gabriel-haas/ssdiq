#pragma once

#include "Intrin.hpp"
#include "Units.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace mean {
inline uint64_t readTSC() {
   return intrin::readTSC();
}
inline uint64_t readTSCfenced() {
   return intrin::readTSCfenced();
}
inline double tscPerNs = 2.4;
inline uint64_t nsToTSC(uint64_t ns) {
   return ns * tscPerNs;
}
inline uint64_t tscDifferenceNs(uint64_t end, uint64_t start) {
   return (end - start) * 1 / tscPerNs;
}
inline uint64_t tscDifferenceUs(uint64_t end, uint64_t start) {
   return tscDifferenceNs(end, start) / KILO;
}
inline uint64_t tscDifferenceMs(uint64_t end, uint64_t start) {
   return tscDifferenceNs(end, start) / MEGA;
}
inline uint64_t tscDifferenceS(uint64_t end, uint64_t start) {
   return tscDifferenceNs(end, start) / GIGA;
}
static auto _staticStartTsc = readTSC();
inline u64 nanoFromTsc(uint64_t tp) {
   return tscDifferenceNs(tp, _staticStartTsc);
}
using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
inline TimePoint getTimePoint() {
   return std::chrono::high_resolution_clock::now();
}
inline uint64_t timePointDifference(TimePoint end, TimePoint start) {
   return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}
inline uint64_t timePointDifferenceUs(TimePoint end, TimePoint start) {
   return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}
inline uint64_t timePointDifferenceMs(TimePoint end, TimePoint start) {
   return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}
static auto _staticStartTimingPoint = getTimePoint();
inline u64 nanoFromTimePoint(TimePoint tp) {
   return timePointDifference(tp, _staticStartTimingPoint);
}
inline float getSeconds() {
   auto tp = getTimePoint();
   return nanoFromTimePoint(tp) * NANO;
}
inline float getRoundSeconds() {
   static auto last = getTimePoint();
   auto now = getTimePoint();
   auto diff = timePointDifference(now, last) * NANO;
   last = now;
   return diff;
}
inline std::string getTimeStampStr() {
   auto now = std::chrono::system_clock::now();
   auto in_time_t = std::chrono::system_clock::to_time_t(now);
   auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
   std::ostringstream oss;
   oss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S")
       << '.' << std::setw(3) << std::setfill('0') << milliseconds.count();
   return oss.str();
}
// -------------------------------------------------------------------------------------
} // namespace mean
// -------------------------------------------------------------------------------------
