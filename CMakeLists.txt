cmake_minimum_required(VERSION 3.3)
project(ZOS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp fat.cpp fat.h)
add_executable(ZOS ${SOURCE_FILES})