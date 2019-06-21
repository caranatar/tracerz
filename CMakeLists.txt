cmake_minimum_required(VERSION 3.9)
project(tracerz)

set(CMAKE_CXX_STANDARD 17)

# Require dot, treat the other components as optional
find_package(Doxygen
        REQUIRED dot
        OPTIONAL_COMPONENTS mscgen dia)

include_directories(json/single_include/nlohmann)
include_directories(Catch2/single_include/catch2)

add_executable(tracerz main.cpp tracerz.h)

set(DOXYGEN_EXCLUDE_PATTERNS ./Catch2/*)
set(DOXYGEN_EXCLUDE_PATTERNS ./json/*)
set(DOXYGEN_OUTPUT_DIRECTORY docs)
doxygen_add_docs(doxygen tracerz.h)