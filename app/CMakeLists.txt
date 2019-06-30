cmake_minimum_required (VERSION 3.2)
project(Lightswitch VERSION 1 LANGUAGES CXX)
set_property(GLOBAL PROPERTY CMAKE_CXX_STANDARD 17 PROPERTY CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(source_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)

include_directories(${source_DIR}/include)

add_library(lightswitch SHARED
        ${source_DIR}/lightswitch.cpp
        ${source_DIR}/core/arm/cpu.cpp

        ${source_DIR}/core/hos/loaders/nro.cpp
)
target_link_libraries(lightswitch ${source_DIR}/lib/${ANDROID_ABI}/libunicorn.a)