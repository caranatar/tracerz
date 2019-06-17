cmake_minimum_required(VERSION 3.9)
project(tracerz)

set(CMAKE_CXX_STANDARD 17)

include_directories(json/single_include/nlohmann)
include_directories(Catch2/single_include/catch2)

add_executable(tracerz main.cpp tracerz.h)