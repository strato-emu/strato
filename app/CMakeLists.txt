cmake_minimum_required(VERSION 3.8)
project(Lightswitch VERSION 1 LANGUAGES CXX)
set_property(GLOBAL PROPERTY CMAKE_CXX_STANDARD 17 PROPERTY CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(BUILD_TESTING OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -flto=full")
add_subdirectory("libraries/tinyxml2")
add_subdirectory("libraries/fmt")

set(source_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)

include_directories(${source_DIR})

add_library(lightswitch SHARED
        ${source_DIR}/lightswitch.cpp
        ${source_DIR}/switch/common.cpp
        ${source_DIR}/switch/nce.cpp
        ${source_DIR}/switch/os.cpp
        ${source_DIR}/switch/loader/nro.cpp
        ${source_DIR}/switch/kernel/ipc.cpp
        ${source_DIR}/switch/kernel/svc.cpp
        ${source_DIR}/switch/kernel/service.cpp
        ${source_DIR}/switch/kernel/types/KProcess.cpp
        ${source_DIR}/switch/kernel/types/KThread.cpp
        )
target_link_libraries(lightswitch fmt tinyxml2 android)
target_compile_options(lightswitch PRIVATE -Wno-c++17-extensions)
