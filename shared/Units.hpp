#pragma once
// -------------------------------------------------------------------------------------
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
// -------------------------------------------------------------------------------------
using std::atomic;
using std::cerr;
using std::cout;
using std::endl;
using std::make_unique;
using std::string;
using std::to_string;
using std::tuple;
using std::unique_ptr;
// -------------------------------------------------------------------------------------
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using u128 = unsigned __int128;
// -------------------------------------------------------------------------------------
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
// -------------------------------------------------------------------------------------
using TINYINT = s8;
using SMALLINT = s16;
using INTEGER = s32;
using UINTEGER = u32;
using DOUBLE = double;
using STRING = string;
using BITMAP = u8;
// -------------------------------------------------------------------------------------
using str = std::string_view;
// -------------------------------------------------------------------------------------
using BytesArray = std::unique_ptr<u8[]>;
// -------------------------------------------------------------------------------------
template <int s>
struct getTheSizeOf;
// -------------------------------------------------------------------------------------
constexpr u64 MSB = u64(1) << 63;
constexpr u64 MSB_MASK = ~(MSB);
constexpr u64 MSB2 = u64(1) << 62;
constexpr u64 MSB2_MASK = ~(MSB2);
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
constexpr u64 TERA = 1000LL * 1000 * 1000 * 1000;
constexpr u64 GIGA = 1000LL * 1000 * 1000;
constexpr u64 MEGA = 1000LL * 1000;
constexpr u64 KILO = 1000LL;
// -------------------------------------------------------------------------------------
constexpr u64 TEBI = 1024LL * 1024 * 1024 * 1024;
constexpr u64 GIBI = 1024LL * 1024 * 1024;
constexpr u64 MEBI = 1024LL * 1024;
constexpr u64 KIBI = 1024LL;
// -------------------------------------------------------------------------------------
constexpr double MILLI = 1e-3;
constexpr double MICRO = 1e-6;
constexpr double NANO = 1e-9;
// -------------------------------------------------------------------------------------
