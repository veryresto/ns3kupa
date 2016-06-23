cmake_minimum_required(VERSION 3.5)
project(multi)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    dce-multi.cc
    wscript)

add_executable(multi ${SOURCE_FILES})