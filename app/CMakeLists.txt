cmake_minimum_required(VERSION 3.8)
project(Lightswitch VERSION 1 LANGUAGES CXX)
set_property(GLOBAL PROPERTY CMAKE_CXX_STANDARD 17 PROPERTY CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(BUILD_TESTS FALSE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
add_subdirectory("libraries/tinyxml2")
add_subdirectory("libraries/fmt")

set(source_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)

include_directories(libraries/unicorn/include)
include_directories(${source_DIR})

add_library(lightswitch SHARED
        ${source_DIR}/lightswitch.cpp
        ${source_DIR}/switch/os/os.cpp
        ${source_DIR}/switch/os/ipc.cpp
        ${source_DIR}/switch/os/kernel.cpp
        ${source_DIR}/switch/os/svc.cpp
        ${source_DIR}/switch/hw/cpu.cpp
        ${source_DIR}/switch/hw/memory.cpp
        ${source_DIR}/switch/common.cpp
        ${source_DIR}/switch/loader/nro.cpp
        )
target_link_libraries(lightswitch ${CMAKE_SOURCE_DIR}/libraries/unicorn/libunicorn.a fmt tinyxml2)
target_compile_options(lightswitch PRIVATE -Wno-c++17-extensions)
