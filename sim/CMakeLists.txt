add_executable(sim sim.cpp)
target_link_libraries(sim PRIVATE shared CLI11::CLI11)