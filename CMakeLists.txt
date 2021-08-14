cmake_minimum_required(VERSION 3.13)
project(fanControl)

set(CMAKE_CXX_STANDARD 20)

add_executable(fanControl main.cpp)

# Link against wiringPi
find_library(WIRINGPI_LIBRARIES wiringPi)
find_library(RUDECONFIG_LIBRARIES rudeconfig)

#include_directories(lib/WiringPi/wiringPi)
include_directories(lib/WiringPi/wiringOP)
include_directories(lib/rudeconfig-5.0.5/src)

target_link_libraries(fanControl ${WIRINGPI_LIBRARIES})
target_link_libraries(fanControl ${RUDECONFIG_LIBRARIES})
