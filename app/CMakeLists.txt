cmake_minimum_required(VERSION 3.8)
project(Skyline VERSION 1 LANGUAGES CXX)

set(BUILD_TESTING OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -flto=full")
add_subdirectory("libraries/tinyxml2")
add_subdirectory("libraries/fmt")

set(source_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)

include_directories(${source_DIR}/skyline)

add_library(skyline SHARED
        ${source_DIR}/main.cpp
        ${source_DIR}/skyline/common.cpp
        ${source_DIR}/skyline/nce.cpp
        ${source_DIR}/skyline/os.cpp
        ${source_DIR}/skyline/loader/nro.cpp
        ${source_DIR}/skyline/kernel/ipc.cpp
        ${source_DIR}/skyline/kernel/svc.cpp
        ${source_DIR}/skyline/kernel/types/KSyncObject.cpp
        ${source_DIR}/skyline/kernel/types/KProcess.cpp
        ${source_DIR}/skyline/kernel/types/KThread.cpp
        ${source_DIR}/skyline/kernel/types/KSharedMemory.cpp
        ${source_DIR}/skyline/kernel/types/KTransferMemory.cpp
        ${source_DIR}/skyline/kernel/types/KPrivateMemory.cpp
        ${source_DIR}/skyline/kernel/services/serviceman.cpp
        ${source_DIR}/skyline/kernel/services/sm/sm.cpp
        ${source_DIR}/skyline/kernel/services/fatal/fatal.cpp
        ${source_DIR}/skyline/kernel/services/set/sys.cpp
        ${source_DIR}/skyline/kernel/services/apm/apm.cpp
        ${source_DIR}/skyline/kernel/services/am/appletOE.cpp
        ${source_DIR}/skyline/kernel/services/hid/hid.cpp
        )
target_link_libraries(skyline fmt tinyxml2)
target_compile_options(skyline PRIVATE -Wno-c++17-extensions)
