cmake_minimum_required(VERSION 3.22)
project(zipfc)

add_library(zipfc zipfc.cpp zipfc.h)
add_custom_target(
        zipfc_cargo
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/cargo_target/release/libzipfc.a
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml ${CMAKE_CURRENT_SOURCE_DIR}/src/lib.rs
        COMMAND env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo_target/ cargo build --release
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(zipfc zipfc_cargo)
target_link_libraries(zipfc PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/cargo_target/release/libzipfc.a)
target_include_directories(zipfc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
