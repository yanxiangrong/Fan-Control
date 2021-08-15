cmake_minimum_required(VERSION 3.13)
project(fanControl)

set(CMAKE_CXX_STANDARD 20)

add_executable(fanControl main.cpp)

find_library(WIRINGPI_LIBRARIES wiringPi)

include_directories(third_party/WiringPi/wiringOP)
include_directories(third_party/rudeconfig-5.0.5/src)

link_directories(third_party/rudeconfig-5.0.5/.libs)

target_link_libraries(fanControl librudeconfig.a)
target_link_libraries(fanControl ${WIRINGPI_LIBRARIES})
