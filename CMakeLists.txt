cmake_minimum_required(VERSION 3.14)
project(tracerz)

set(CMAKE_CXX_STANDARD 17)

include_directories(json/single_include/nlohmann)

add_executable(tracerz main.cpp tracerz.h)