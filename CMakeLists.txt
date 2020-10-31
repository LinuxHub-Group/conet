cmake_minimum_required(VERSION 3.17)
project(conet)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_library(conet INTERFACE)
target_include_directories(conet INTERFACE ${PROJECT_SOURCE_DIR})
include_directories(${INC})

include_directories(${PROJECT_SOURCE_DIR})
add_subdirectory(examples)
