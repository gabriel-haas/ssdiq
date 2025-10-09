add_executable(zipf zipf.cpp)

target_link_libraries(zipf
    PRIVATE
        shared
        apache_rng
)