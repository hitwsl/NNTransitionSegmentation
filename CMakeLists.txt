project(NNTransitionSegmention)
cmake_minimum_required(VERSION 2.7 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    SET( CMAKE_CXX_FLAGS  "-w -msse3 -funroll-loops  -std=c++11 -O0" )
else()
    SET( CMAKE_CXX_FLAGS  "-w -msse3 -funroll-loops  -std=c++11 -O3" )
endif()

include_directories(${EIGEN3_INCLUDE_DIR})
include_directories(${N3L_INCLUDE_DIR})

add_subdirectory(src)
