cmake_minimum_required(VERSION 3.8)
project(Skyline LANGUAGES CXX ASM VERSION 0.3)

set(BUILD_TESTS OFF)
set(BUILD_TESTING OFF)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Shared Libraries" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(source_DIR ${CMAKE_SOURCE_DIR}/src/main/cpp)
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto=full -Wno-unused-command-line-argument")
if (uppercase_CMAKE_BUILD_TYPE STREQUAL "RELEASE")
    add_compile_definitions(NDEBUG)
endif()

set(CMAKE_POLICY_DEFAULT_CMP0048 OLD)
add_subdirectory("libraries/tinyxml2")
add_subdirectory("libraries/fmt")
add_subdirectory("libraries/oboe")
add_subdirectory("libraries/lz4/contrib/cmake_unofficial")
include_directories("libraries/lz4/lib")
include_directories("libraries/oboe/include")
include_directories("libraries/vkhpp/include")
set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)

include_directories(${source_DIR}/skyline)

add_library(skyline SHARED
        ${source_DIR}/emu_jni.cpp
        ${source_DIR}/loader_jni.cpp
        ${source_DIR}/skyline/common.cpp
        ${source_DIR}/skyline/nce/guest.S
        ${source_DIR}/skyline/nce/guest.cpp
        ${source_DIR}/skyline/nce.cpp
        ${source_DIR}/skyline/jvm.cpp
        ${source_DIR}/skyline/audio.cpp
        ${source_DIR}/skyline/audio/track.cpp
        ${source_DIR}/skyline/audio/resampler.cpp
        ${source_DIR}/skyline/audio/adpcm_decoder.cpp
        ${source_DIR}/skyline/gpu.cpp
        ${source_DIR}/skyline/gpu/macro_interpreter.cpp
        ${source_DIR}/skyline/gpu/memory_manager.cpp
        ${source_DIR}/skyline/gpu/gpfifo.cpp
        ${source_DIR}/skyline/gpu/syncpoint.cpp
        ${source_DIR}/skyline/gpu/texture.cpp
        ${source_DIR}/skyline/gpu/engines/maxwell_3d.cpp
        ${source_DIR}/skyline/input.cpp
        ${source_DIR}/skyline/input/npad.cpp
        ${source_DIR}/skyline/input/npad_device.cpp
        ${source_DIR}/skyline/input/touch.cpp
        ${source_DIR}/skyline/os.cpp
        ${source_DIR}/skyline/loader/loader.cpp
        ${source_DIR}/skyline/loader/nro.cpp
        ${source_DIR}/skyline/loader/nso.cpp
        ${source_DIR}/skyline/loader/nca.cpp
        ${source_DIR}/skyline/loader/nsp.cpp
        ${source_DIR}/skyline/kernel/memory.cpp
        ${source_DIR}/skyline/kernel/ipc.cpp
        ${source_DIR}/skyline/kernel/svc.cpp
        ${source_DIR}/skyline/kernel/types/KProcess.cpp
        ${source_DIR}/skyline/kernel/types/KThread.cpp
        ${source_DIR}/skyline/kernel/types/KSharedMemory.cpp
        ${source_DIR}/skyline/kernel/types/KTransferMemory.cpp
        ${source_DIR}/skyline/kernel/types/KPrivateMemory.cpp
        ${source_DIR}/skyline/services/serviceman.cpp
        ${source_DIR}/skyline/services/common/parcel.cpp
        ${source_DIR}/skyline/services/sm/IUserInterface.cpp
        ${source_DIR}/skyline/services/fatalsrv/IService.cpp
        ${source_DIR}/skyline/services/audio/IAudioOutManager.cpp
        ${source_DIR}/skyline/services/audio/IAudioOut.cpp
        ${source_DIR}/skyline/services/audio/IAudioDevice.cpp
        ${source_DIR}/skyline/services/audio/IAudioRendererManager.cpp
        ${source_DIR}/skyline/services/audio/IAudioRenderer/IAudioRenderer.cpp
        ${source_DIR}/skyline/services/audio/IAudioRenderer/voice.cpp
        ${source_DIR}/skyline/services/audio/IAudioRenderer/memory_pool.cpp
        ${source_DIR}/skyline/services/settings/ISettingsServer.cpp
        ${source_DIR}/skyline/services/settings/ISystemSettingsServer.cpp
        ${source_DIR}/skyline/services/apm/IManager.cpp
        ${source_DIR}/skyline/services/apm/ISession.cpp
        ${source_DIR}/skyline/services/am/IAllSystemAppletProxiesService.cpp
        ${source_DIR}/skyline/services/am/IApplicationProxyService.cpp
        ${source_DIR}/skyline/services/am/proxy/base_proxy.cpp
        ${source_DIR}/skyline/services/am/proxy/IApplicationProxy.cpp
        ${source_DIR}/skyline/services/am/proxy/ILibraryAppletProxy.cpp
        ${source_DIR}/skyline/services/am/proxy/IOverlayAppletProxy.cpp
        ${source_DIR}/skyline/services/am/proxy/ISystemAppletProxy.cpp
        ${source_DIR}/skyline/services/am/controller/IAppletCommonFunctions.cpp
        ${source_DIR}/skyline/services/am/controller/IApplicationFunctions.cpp
        ${source_DIR}/skyline/services/am/controller/IAudioController.cpp
        ${source_DIR}/skyline/services/am/controller/ICommonStateGetter.cpp
        ${source_DIR}/skyline/services/am/controller/IDebugFunctions.cpp
        ${source_DIR}/skyline/services/am/controller/IDisplayController.cpp
        ${source_DIR}/skyline/services/am/controller/ILibraryAppletCreator.cpp
        ${source_DIR}/skyline/services/am/controller/ISelfController.cpp
        ${source_DIR}/skyline/services/am/controller/IWindowController.cpp
        ${source_DIR}/skyline/services/am/storage/IStorage.cpp
        ${source_DIR}/skyline/services/am/storage/IStorageAccessor.cpp
        ${source_DIR}/skyline/services/am/applet/ILibraryAppletAccessor.cpp
        ${source_DIR}/skyline/services/hid/IHidServer.cpp
        ${source_DIR}/skyline/services/hid/IAppletResource.cpp
        ${source_DIR}/skyline/services/hid/IActiveVibrationDeviceList.cpp
        ${source_DIR}/skyline/services/timesrv/IStaticService.cpp
        ${source_DIR}/skyline/services/timesrv/ISystemClock.cpp
        ${source_DIR}/skyline/services/timesrv/ISteadyClock.cpp
        ${source_DIR}/skyline/services/timesrv/ITimeZoneService.cpp
        ${source_DIR}/skyline/services/fssrv/IFileSystemProxy.cpp
        ${source_DIR}/skyline/services/fssrv/IFileSystem.cpp
        ${source_DIR}/skyline/services/fssrv/IFile.cpp
        ${source_DIR}/skyline/services/fssrv/IStorage.cpp
        ${source_DIR}/skyline/services/nvdrv/INvDrvServices.cpp
        ${source_DIR}/skyline/services/nvdrv/devices/nvmap.cpp
        ${source_DIR}/skyline/services/nvdrv/devices/nvhost_ctrl_gpu.cpp
        ${source_DIR}/skyline/services/nvdrv/devices/nvhost_ctrl.cpp
        ${source_DIR}/skyline/services/nvdrv/devices/nvhost_channel.cpp
        ${source_DIR}/skyline/services/nvdrv/devices/nvhost_as_gpu.cpp
        ${source_DIR}/skyline/services/nvdrv/devices/nvhost_syncpoint.cpp
        ${source_DIR}/skyline/services/hosbinder/IHOSBinderDriver.cpp
        ${source_DIR}/skyline/services/visrv/IDisplayService.cpp
        ${source_DIR}/skyline/services/visrv/IApplicationDisplayService.cpp
        ${source_DIR}/skyline/services/visrv/IManagerDisplayService.cpp
        ${source_DIR}/skyline/services/visrv/IManagerRootService.cpp
        ${source_DIR}/skyline/services/visrv/ISystemDisplayService.cpp
        ${source_DIR}/skyline/services/pl/IPlatformServiceManager.cpp
        ${source_DIR}/skyline/services/aocsrv/IAddOnContentManager.cpp
        ${source_DIR}/skyline/services/pctl/IParentalControlServiceFactory.cpp
        ${source_DIR}/skyline/services/pctl/IParentalControlService.cpp
        ${source_DIR}/skyline/services/lm/ILogService.cpp
        ${source_DIR}/skyline/services/lm/ILogger.cpp
        ${source_DIR}/skyline/services/account/IAccountServiceForApplication.cpp
        ${source_DIR}/skyline/services/account/IManagerForApplication.cpp
        ${source_DIR}/skyline/services/account/IProfile.cpp
        ${source_DIR}/skyline/services/friends/IServiceCreator.cpp
        ${source_DIR}/skyline/services/friends/IFriendService.cpp
        ${source_DIR}/skyline/services/friends/INotificationService.cpp
        ${source_DIR}/skyline/services/nfp/IUserManager.cpp
        ${source_DIR}/skyline/services/nfp/IUser.cpp
        ${source_DIR}/skyline/services/nifm/IStaticService.cpp
        ${source_DIR}/skyline/services/nifm/IGeneralService.cpp
        ${source_DIR}/skyline/services/nifm/IRequest.cpp
        ${source_DIR}/skyline/services/socket/bsd/IClient.cpp
        ${source_DIR}/skyline/services/ssl/ISslService.cpp
        ${source_DIR}/skyline/services/ssl/ISslContext.cpp
        ${source_DIR}/skyline/services/prepo/IPrepoService.cpp
        ${source_DIR}/skyline/vfs/os_filesystem.cpp
        ${source_DIR}/skyline/vfs/partition_filesystem.cpp
        ${source_DIR}/skyline/vfs/rom_filesystem.cpp
        ${source_DIR}/skyline/vfs/os_backing.cpp
        ${source_DIR}/skyline/vfs/nacp.cpp
        ${source_DIR}/skyline/vfs/nca.cpp
        )

target_link_libraries(skyline vulkan android fmt tinyxml2 oboe lz4_static)
set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=c++2a")
target_compile_options(skyline PRIVATE -Wno-c++17-extensions -Wall -Wno-reorder -Wno-missing-braces -Wno-unused-variable -Wno-unused-private-field)
