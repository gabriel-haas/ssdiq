include(FetchContent)

FetchContent_Declare(
  xxhash_src
  GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
  GIT_TAG v0.8.3
)

FetchContent_MakeAvailable(xxhash_src)

add_library(xxhash STATIC
    ${xxhash_src_SOURCE_DIR}/xxhash.c
)
target_include_directories(xxhash PUBLIC
    ${xxhash_src_SOURCE_DIR}
)