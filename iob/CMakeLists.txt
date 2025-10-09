file(GLOB_RECURSE SRC_FILES *.cpp)
add_executable(iob ${SRC_FILES})

target_include_directories(iob PRIVATE ${xxhash_SOURCE_DIR})



find_library(URING_LIBRARY uring)
find_library(AIO_LIBRARY aio)
find_library(NVME_LIBRARY nvme)
if(URING_LIBRARY)
    list(APPEND IOB_LIBS ${URING_LIBRARY})
else()
    message(STATUS "iob: URING library not found")
endif()
if(AIO_LIBRARY)
    list(APPEND IOB_LIBS ${AIO_LIBRARY})
else()
    message(STATUS "iob: AIO library not found")
endif()
if(NVME_LIBRARY)
    list(APPEND IOB_LIBS ${NVME_LIBRARY})
else()
    message(STATUS "iob: NVME library not found")
endif()

target_link_libraries(iob
    PRIVATE
        shared
        xxhash
        CLI11::CLI11
        ${IOB_LIBS}
)
