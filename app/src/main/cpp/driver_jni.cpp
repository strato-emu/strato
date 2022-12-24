// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <jni.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <vulkan/vulkan_raii.hpp>
#include <adrenotools/driver.h>
#include "skyline/common/signal.h"
#include "skyline/common/utils.h"

extern "C" JNIEXPORT jobjectArray JNICALL Java_emu_skyline_utils_GpuDriverHelper_00024Companion_getSystemDriverInfo(JNIEnv *env, jobject) {
    auto libvulkanHandle{dlopen("libvulkan.so", RTLD_NOW)};

    vk::raii::Context vkContext{reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(libvulkanHandle, "vkGetInstanceProcAddr"))};
    vk::raii::Instance vkInstance{vkContext, vk::InstanceCreateInfo{}};
    vk::raii::PhysicalDevice physicalDevice{std::move(vk::raii::PhysicalDevices(vkInstance).front())}; // Get the first device as we aren't expecting multiple GPUs

    auto deviceProperties2{physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>()};
    auto properties{deviceProperties2.get<vk::PhysicalDeviceProperties2>().properties};

    auto driverId{vk::to_string(deviceProperties2.get<vk::PhysicalDeviceDriverProperties>().driverID)};
    auto driverVersion{skyline::util::Format("{}.{}.{}", VK_API_VERSION_MAJOR(properties.driverVersion), VK_API_VERSION_MINOR(properties.driverVersion), VK_API_VERSION_PATCH(properties.driverVersion))};

    auto array = env->NewObjectArray(2, env->FindClass("java/lang/String"), nullptr);
    env->SetObjectArrayElement(array, 0, env->NewStringUTF(driverId.c_str()));
    env->SetObjectArrayElement(array, 1, env->NewStringUTF(driverVersion.c_str()));

    return array;
}

static bool CheckKgslPresent() {
    constexpr auto KgslPath{"/dev/kgsl-3d0"};

    return access(KgslPath, F_OK) == 0;
}

extern "C" JNIEXPORT jboolean JNICALL Java_emu_skyline_utils_GpuDriverHelper_00024Companion_supportsCustomDriverLoading(JNIEnv *env, jobject instance) {
    // If the KGSL device exists custom drivers can be loaded using adrenotools
    return CheckKgslPresent();
}

extern "C" JNIEXPORT jboolean JNICALL Java_emu_skyline_utils_GpuDriverHelper_00024Companion_supportsForceMaxGpuClocks(JNIEnv *env, jobject instance) {
    // If the KGSL device exists adrenotools can be used to set GPU turbo mode
    return CheckKgslPresent();
}

extern "C" JNIEXPORT void JNICALL Java_emu_skyline_utils_GpuDriverHelper_00024Companion_forceMaxGpuClocks(JNIEnv *env, jobject instance, jboolean enable) {
    adrenotools_set_turbo(enable);
}
