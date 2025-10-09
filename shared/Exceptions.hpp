#pragma once
// -------------------------------------------------------------------------------------
#include "Units.hpp"
// -------------------------------------------------------------------------------------
#include <csignal>
#include <exception>
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
inline void posix_check(bool expr, string msg = "") {
   if (!(expr)) {
      perror("posix_check failed");
      perror(msg.c_str());
      assert(false);
   }
}
// #define posix_check(expr) //  if (!(expr)) {          //    perror(#expr);        //    assert(false);        //  }
//--------------------------------------------------------------------------------------
#define Generic_Exception(name)                                                                                   \
   struct name : public std::exception {                                                                          \
      const string msg;                                                                                           \
      explicit name() : msg(#name) { printf("Throwing exception: %s\n", #name); }                                 \
      explicit name(const string& msg) : msg(msg) { printf("Throwing exception: %s(%s)\n", #name, msg.c_str()); } \
      ~name() = default;                                                                                          \
      virtual const char* what() const noexcept { return msg.c_str(); }                                           \
   };                                                                                                             \
//--------------------------------------------------------------------------------------
Generic_Exception(GenericException);
Generic_Exception(EnsureFailed);
Generic_Exception(UnReachable);
Generic_Exception(TODO);
// -------------------------------------------------------------------------------------
#define UNREACHABLE() throw UnReachable(string(__FILE__) + ":" + string(to_string(__LINE__)));
// -------------------------------------------------------------------------------------
#define ensurem(e, msg)                                                        \
   if (__builtin_expect(!(e), 0)) {                                            \
      throw_fun(string(__func__), string(__FILE__), to_string(__LINE__), msg); \
   }
#define ensure(e)                                                         \
   if (__builtin_expect(!(e), 0)) {                                       \
      throw_fun(string(__func__), string(__FILE__), to_string(__LINE__)); \
   }
#define null_check(e) ensure(e);
#define null_checkm(e, msg) ensurem(e, msg);
inline void throw_fun(string func, string file, string line, string msg = "") {
   throw EnsureFailed(func + " in " + file + "@" + line + ": " + msg);
}
// -------------------------------------------------------------------------------------
template <typename T>
inline void DO_NOT_OPTIMIZE(T const& value) {
#if defined(__clang__)
   asm volatile("" : : "g"(value) : "memory");
#else
   asm volatile("" : : "i,r,m"(value) : "memory");
#endif
}
