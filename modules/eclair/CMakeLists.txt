cmake_minimum_required(VERSION 3.10)
project(EclairModule)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

include_directories(../../include)
include_directories(../..)

add_library(eclair_module STATIC module.cpp)

set_target_properties(eclair_module PROPERTIES OUTPUT_NAME module)

target_link_libraries(eclair_module ${JNI_LIBRARIES})

