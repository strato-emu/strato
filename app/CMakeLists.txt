cmake_minimum_required(VERSION 3.8)
project(Skyline VERSION 0.3)

set(BUILD_TESTS OFF)
set(BUILD_TESTING OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(source_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto=full -Wno-unused-command-line-argument")
enable_language(ASM)
if (uppercase_CMAKE_BUILD_TYPE STREQUAL "RELEASE")
    add_compile_definitions(NDEBUG)
endif()

set(CMAKE_POLICY_DEFAULT_CMP0048 OLD)
add_subdirectory("libraries/tinyxml2")
add_subdirectory("libraries/fmt")
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)

include_directories(${source_DIR}/skyline)

add_library(skyline SHARED
        ${source_DIR}/main.cpp
        ${source_DIR}/skyline/common.cpp
        ${source_DIR}/skyline/nce/guest.S
        ${source_DIR}/skyline/nce/guest.cpp
        ${source_DIR}/skyline/nce.cpp
        ${source_DIR}/skyline/jvm.cpp
        ${source_DIR}/skyline/gpu.cpp
        ${source_DIR}/skyline/gpu/display.cpp
        ${source_DIR}/skyline/gpu/parcel.cpp
        ${source_DIR}/skyline/gpu/devices/nvhost_ctrl.cpp
        ${source_DIR}/skyline/gpu/devices/nvhost_ctrl_gpu.cpp
        ${source_DIR}/skyline/gpu/devices/nvhost_channel.cpp
        ${source_DIR}/skyline/gpu/devices/nvmap.cpp
        ${source_DIR}/skyline/gpu/devices/nvhost_as_gpu.cpp
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
        ${source_DIR}/skyline/services/serviceman.cpp
        ${source_DIR}/skyline/services/sm/sm.cpp
        ${source_DIR}/skyline/services/fatal/fatal.cpp
        ${source_DIR}/skyline/services/set/sys.cpp
        ${source_DIR}/skyline/services/apm/apm.cpp
        ${source_DIR}/skyline/services/am/applet.cpp
        ${source_DIR}/skyline/services/am/appletController.cpp
        ${source_DIR}/skyline/services/hid/hid.cpp
        ${source_DIR}/skyline/services/time/time.cpp
        ${source_DIR}/skyline/services/fs/fs.cpp
        ${source_DIR}/skyline/services/nvdrv/nvdrv.cpp
        ${source_DIR}/skyline/services/nvnflinger/dispdrv.cpp
        ${source_DIR}/skyline/services/vi/vi_m.cpp
        )

target_link_libraries(skyline vulkan GLESv3 EGL android fmt tinyxml2)
target_compile_options(skyline PRIVATE -Wno-c++17-extensions)
