add_library(shared INTERFACE)

target_include_directories(shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(shared INTERFACE
    apache_rng
)
