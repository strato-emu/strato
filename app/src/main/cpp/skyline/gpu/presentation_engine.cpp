// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window_jni.h>
#include <android/choreographer.h>
#include <common/settings.h>
#include <jvm.h>
#include <gpu.h>
#include "presentation_engine.h"
#include "texture/format.h"

extern skyline::i32 Fps;
extern skyline::i32 FrameTime;

namespace skyline::gpu {
    PresentationEngine::PresentationEngine(const DeviceState &state, GPU &gpu) : state(state), gpu(gpu), acquireFence(gpu.vkDevice, vk::FenceCreateInfo{}), presentationTrack(static_cast<u64>(trace::TrackIds::Presentation), perfetto::ProcessTrack::Current()), choreographerThread(&PresentationEngine::ChoreographerThread, this), vsyncEvent(std::make_shared<kernel::type::KEvent>(state, true)) {
        auto desc{presentationTrack.Serialize()};
        desc.set_name("Presentation");
        perfetto::TrackEvent::SetTrackDescriptor(presentationTrack, desc);
    }

    PresentationEngine::~PresentationEngine() {
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr))
            env->DeleteGlobalRef(jSurface);

        if (choreographerThread.joinable()) {
            if (choreographerLooper)
                ALooper_wake(choreographerLooper);
            choreographerThread.join();
        }
    }

    /**
     * @url https://developer.android.com/ndk/reference/group/choreographer#achoreographer_framecallback
     */
    void ChoreographerCallback(long frameTimeNanos, kernel::type::KEvent *vsyncEvent) {
        vsyncEvent->Signal();
        AChoreographer_postFrameCallback(AChoreographer_getInstance(), reinterpret_cast<AChoreographer_frameCallback>(&ChoreographerCallback), vsyncEvent);
    }

    void PresentationEngine::ChoreographerThread() {
        choreographerLooper = ALooper_prepare(0);
        AChoreographer_postFrameCallback(AChoreographer_getInstance(), reinterpret_cast<AChoreographer_frameCallback>(&ChoreographerCallback), vsyncEvent.get());
        ALooper_pollAll(-1, nullptr, nullptr, nullptr); // Will block and process callbacks till ALooper_wake() is called
    }

    service::hosbinder::NativeWindowTransform GetAndroidTransform(vk::SurfaceTransformFlagBitsKHR transform) {
        using NativeWindowTransform = service::hosbinder::NativeWindowTransform;
        switch (transform) {
            case vk::SurfaceTransformFlagBitsKHR::eIdentity:
            case vk::SurfaceTransformFlagBitsKHR::eInherit:
                return NativeWindowTransform::Identity;
            case vk::SurfaceTransformFlagBitsKHR::eRotate90:
                return NativeWindowTransform::Rotate90;
            case vk::SurfaceTransformFlagBitsKHR::eRotate180:
                return NativeWindowTransform::Rotate180;
            case vk::SurfaceTransformFlagBitsKHR::eRotate270:
                return NativeWindowTransform::Rotate270;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror:
                return NativeWindowTransform::MirrorHorizontal;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate90:
                return NativeWindowTransform::MirrorHorizontalRotate90;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate180:
                return NativeWindowTransform::MirrorVertical;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate270:
                return NativeWindowTransform::MirrorVerticalRotate90;
        }
    }

    void PresentationEngine::UpdateSwapchain(texture::Format format, texture::Dimensions extent) {
        auto minImageCount{std::max(vkSurfaceCapabilities.minImageCount, state.settings->forceTripleBuffering ? 3U : 2U)};
        if (minImageCount > MaxSwapchainImageCount)
            throw exception("Requesting swapchain with higher image count ({}) than maximum slot count ({})", minImageCount, MaxSwapchainImageCount);

        const auto &capabilities{vkSurfaceCapabilities};
        if (minImageCount < capabilities.minImageCount || (capabilities.maxImageCount && minImageCount > capabilities.maxImageCount))
            throw exception("Cannot update swapchain to accomodate image count: {} ({}-{})", minImageCount, capabilities.minImageCount, capabilities.maxImageCount);
        else if (capabilities.minImageExtent.height > extent.height || capabilities.minImageExtent.width > extent.width || capabilities.maxImageExtent.height < extent.height || capabilities.maxImageExtent.width < extent.width)
            throw exception("Cannot update swapchain to accomodate image extent: {}x{} ({}x{}-{}x{})", extent.width, extent.height, capabilities.minImageExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);

        if (swapchainFormat != format) {
            auto formats{gpu.vkPhysicalDevice.getSurfaceFormatsKHR(**vkSurface)};
            if (std::find(formats.begin(), formats.end(), vk::SurfaceFormatKHR{format, vk::ColorSpaceKHR::eSrgbNonlinear}) == formats.end())
                throw exception("Surface doesn't support requested image format '{}' with colorspace '{}'", vk::to_string(format), vk::to_string(vk::ColorSpaceKHR::eSrgbNonlinear));
        }

        constexpr vk::ImageUsageFlags presentUsage{vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst};
        if ((capabilities.supportedUsageFlags & presentUsage) != presentUsage)
            throw exception("Swapchain doesn't support image usage '{}': {}", vk::to_string(presentUsage), vk::to_string(capabilities.supportedUsageFlags));

        vkSwapchain.emplace(gpu.vkDevice, vk::SwapchainCreateInfoKHR{
            .surface = **vkSurface,
            .minImageCount = minImageCount,
            .imageFormat = format,
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = presentUsage,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit,
            .presentMode = state.settings->disableFrameThrottling ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo,
            .clipped = true,
        });

        auto vkImages{vkSwapchain->getImages()};
        if (vkImages.size() > MaxSwapchainImageCount)
            throw exception("Swapchain has higher image count ({}) than maximum slot count ({})", minImageCount, MaxSwapchainImageCount);
        state.logger->Error("Buffer Count: {}", vkImages.size());

        for (size_t index{}; index < vkImages.size(); index++) {
            auto &slot{images[index]};
            slot = std::make_shared<Texture>(*state.gpu, vkImages[index], extent, format::GetFormat(format), vk::ImageLayout::eUndefined, vk::ImageTiling::eOptimal);
            slot->TransitionLayout(vk::ImageLayout::ePresentSrcKHR);
        }
        for (size_t index{vkImages.size()}; index < MaxSwapchainImageCount; index++)
            // We need to clear all the slots which aren't filled, keeping around stale slots could lead to issues
            images[index] = {};

        swapchainFormat = format;
        swapchainExtent = extent;
    }

    void PresentationEngine::UpdateSurface(jobject newSurface) {
        std::lock_guard guard(mutex);

        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr)) {
            env->DeleteGlobalRef(jSurface);
            jSurface = nullptr;
        }
        if (!env->IsSameObject(newSurface, nullptr))
            jSurface = env->NewGlobalRef(newSurface);

        vkSwapchain.reset();

        if (jSurface) {
            vkSurface.emplace(gpu.vkInstance, vk::AndroidSurfaceCreateInfoKHR{
                .window = ANativeWindow_fromSurface(env, jSurface),
            });
            if (!gpu.vkPhysicalDevice.getSurfaceSupportKHR(gpu.vkQueueFamilyIndex, **vkSurface))
                throw exception("Vulkan Queue doesn't support presentation with surface");
            vkSurfaceCapabilities = gpu.vkPhysicalDevice.getSurfaceCapabilitiesKHR(**vkSurface);

            if (swapchainExtent && swapchainFormat)
                UpdateSwapchain(swapchainFormat, swapchainExtent);

            surfaceCondition.notify_all();
        } else {
            vkSurface.reset();
        }
    }

    void PresentationEngine::Present(const std::shared_ptr<Texture> &texture, u64 presentId) {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });

        if (texture->format != swapchainFormat || texture->dimensions != swapchainExtent)
            UpdateSwapchain(texture->format, texture->dimensions);

        std::pair<vk::Result, u32> nextImage;
        while (nextImage = vkSwapchain->acquireNextImage(std::numeric_limits<u64>::max(), {}, *acquireFence), nextImage.first != vk::Result::eSuccess) [[unlikely]] {
            if (nextImage.first == vk::Result::eSuboptimalKHR)
                surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
            else
                throw exception("vkAcquireNextImageKHR returned an unhandled result '{}'", vk::to_string(nextImage.first));
        }

        static_cast<void>(gpu.vkDevice.waitForFences(*acquireFence, true, std::numeric_limits<u64>::max()));
        images.at(nextImage.second)->CopyFrom(texture);

        {
            std::lock_guard queueLock(gpu.queueMutex);
            static_cast<void>(gpu.vkQueue.presentKHR(vk::PresentInfoKHR{
                .swapchainCount = 1,
                .pSwapchains = &**vkSwapchain,
                .pImageIndices = &nextImage.second,
            })); // We explicitly discard the result here as suboptimal images are expected when the game doesn't respect the transform hint
        }

        if (frameTimestamp) {
            auto now{util::GetTimeNs()};
            FrameTime = static_cast<u32>((now - frameTimestamp) / 10000); // frametime / 100 is the real ms value, this is to retain the first two decimals
            Fps = static_cast<u16>(constant::NsInSecond / (now - frameTimestamp));

            TRACE_EVENT_INSTANT("gpu", "Present", presentationTrack, "FrameTimeNs", now - frameTimestamp, "Fps", Fps);

            frameTimestamp = now;
        } else {
            frameTimestamp = util::GetTimeNs();
        }
    }

    service::hosbinder::NativeWindowTransform PresentationEngine::GetTransformHint() {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
        return GetAndroidTransform(vkSurfaceCapabilities.currentTransform);
    }
}
