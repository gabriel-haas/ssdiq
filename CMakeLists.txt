cmake_minimum_required(VERSION 3.19)
project(ssdiq)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# enable sanitizers for debug builds
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3") # -g -fno-omit-frame-pointer -finline-limit=1")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -g -fno-omit-frame-pointer -finline-limit=1")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer")

add_compile_options(-Wall -Wno-sign-compare -Wno-comment)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_CXX_CLANG_TIDY "clang-tidy")

# libraries
#include(${CMAKE_SOURCE_DIR}/libs/liburing.cmake)
include(${CMAKE_SOURCE_DIR}/libs/xxhash.cmake)
include(${CMAKE_SOURCE_DIR}/libs/apache_rng.cmake)
include(${CMAKE_SOURCE_DIR}/libs/cli11.cmake)

add_subdirectory(shared)
add_subdirectory(sim)
add_subdirectory(iob)
#add_subdirectory(zipf)
