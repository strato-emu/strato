// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window_jni.h>
#include <gpu.h>
#include <jvm.h>
#include "presentation_engine.h"

extern skyline::i32 Fps;
extern skyline::i32 FrameTime;

namespace skyline::gpu {
    PresentationEngine::PresentationEngine(const DeviceState &state, GPU &gpu) : state(state), gpu(gpu), vsyncEvent(std::make_shared<kernel::type::KEvent>(state, true)), bufferEvent(std::make_shared<kernel::type::KEvent>(state, true)), presentationTrack(static_cast<u64>(trace::TrackIds::Presentation), perfetto::ProcessTrack::Current()) {
        auto desc{presentationTrack.Serialize()};
        desc.set_name("Presentation");
        perfetto::TrackEvent::SetTrackDescriptor(presentationTrack, desc);
    }

    PresentationEngine::~PresentationEngine() {
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr))
            env->DeleteGlobalRef(jSurface);
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

    void PresentationEngine::UpdateSwapchain(u16 imageCount, vk::Format imageFormat, vk::Extent2D imageExtent, bool newSurface) {
        if (!imageCount)
            return;
        else if (imageCount > service::hosbinder::GraphicBufferProducer::MaxSlotCount)
            throw exception("Requesting swapchain with higher image count ({}) than maximum slot count ({})", imageCount, service::hosbinder::GraphicBufferProducer::MaxSlotCount);

        const auto &capabilities{vkSurfaceCapabilities};
        if (imageCount < capabilities.minImageCount || (capabilities.maxImageCount && imageCount > capabilities.maxImageCount))
            throw exception("Cannot update swapchain to accomodate image count: {} ({}-{})", imageCount, capabilities.minImageCount, capabilities.maxImageCount);
        if (capabilities.minImageExtent.height > imageExtent.height || capabilities.minImageExtent.width > imageExtent.width || capabilities.maxImageExtent.height < imageExtent.height || capabilities.maxImageExtent.width < imageExtent.width)
            throw exception("Cannot update swapchain to accomodate image extent: {}x{} ({}x{}-{}x{})", imageExtent.width, imageExtent.height, capabilities.minImageExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);

        if (swapchain.imageFormat != imageFormat || newSurface) {
            auto formats{gpu.vkPhysicalDevice.getSurfaceFormatsKHR(**vkSurface)};
            if (std::find(formats.begin(), formats.end(), vk::SurfaceFormatKHR{imageFormat, vk::ColorSpaceKHR::eSrgbNonlinear}) == formats.end())
                throw exception("Surface doesn't support requested image format '{}' with colorspace '{}'", vk::to_string(imageFormat), vk::to_string(vk::ColorSpaceKHR::eSrgbNonlinear));
        }

        constexpr vk::ImageUsageFlags presentUsage{vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst};
        if ((capabilities.supportedUsageFlags & presentUsage) != presentUsage)
            throw exception("Swapchain doesn't support image usage '{}': {}", vk::to_string(presentUsage), vk::to_string(capabilities.supportedUsageFlags));

        vkSwapchain = vk::raii::SwapchainKHR(gpu.vkDevice, vk::SwapchainCreateInfoKHR{
            .surface = **vkSurface,
            .minImageCount = imageCount,
            .imageFormat = imageFormat,
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = imageExtent,
            .imageArrayLayers = 1,
            .imageUsage = presentUsage,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit,
            .presentMode = vk::PresentModeKHR::eFifo,
            .clipped = false,
            .oldSwapchain = vkSwapchain ? **vkSwapchain : vk::SwapchainKHR{},
        });

        auto vkImages{vkSwapchain->getImages()};
        for (u16 slot{}; slot < imageCount; slot++) {
            auto &vkImage{vkImages[slot]};
            swapchain.vkImages[slot] = vkImage;
            auto &image{swapchain.textures[slot]};
            if (image) {
                std::scoped_lock lock(*image);
                image->SwapBacking(vkImage);
                image->TransitionLayout(vk::ImageLayout::ePresentSrcKHR);
                image->SynchronizeHost(); // Synchronize the new host backing with guest memory
            }
        }
        swapchain.imageCount = imageCount;
        swapchain.imageFormat = imageFormat;
        swapchain.imageExtent = imageExtent;
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

        if (vkSwapchain) {
            for (u16 slot{}; slot < swapchain.imageCount; slot++) {
                auto &image{swapchain.textures[slot]};
                if (image) {
                    std::scoped_lock lock(*image);
                    image->SynchronizeGuest(); // Synchronize host backing to guest memory prior to being destroyed
                    image->SwapBacking(nullptr);
                }
            }
            swapchain.vkImages = {};
            vkSwapchain.reset();
        }

        if (jSurface) {
            vkSurface.emplace(gpu.vkInstance, vk::AndroidSurfaceCreateInfoKHR{
                .window = ANativeWindow_fromSurface(env, jSurface),
            });
            if (!gpu.vkPhysicalDevice.getSurfaceSupportKHR(gpu.vkQueueFamilyIndex, **vkSurface))
                throw exception("Vulkan Queue doesn't support presentation with surface");
            vkSurfaceCapabilities = gpu.vkPhysicalDevice.getSurfaceCapabilitiesKHR(**vkSurface);

            UpdateSwapchain(swapchain.imageCount, swapchain.imageFormat, swapchain.imageExtent, true);

            surfaceCondition.notify_all();
        } else {
            vkSurface.reset();
        }
    }

    std::shared_ptr<Texture> PresentationEngine::CreatePresentationTexture(const std::shared_ptr<GuestTexture> &texture, u8 slot) {
        std::lock_guard guard(mutex);
        if (swapchain.imageCount <= slot && slot + 1 >= vkSurfaceCapabilities.minImageCount)
            UpdateSwapchain(slot + 1, texture->format.vkFormat, texture->dimensions);
        auto host{texture->InitializeTexture(swapchain.vkImages.at(slot), vk::ImageTiling::eOptimal)};
        swapchain.textures[slot] = host;
        return host;
    }

    service::hosbinder::AndroidStatus PresentationEngine::GetFreeTexture(bool async, i32 &slot) {
        using AndroidStatus = service::hosbinder::AndroidStatus;

        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
        if (swapchain.dequeuedCount < swapchain.imageCount) {
            static vk::raii::Fence fence(gpu.vkDevice, vk::FenceCreateInfo{});
            auto timeout{async ? 0ULL : std::numeric_limits<u64>::max()}; // We cannot block for a buffer to be retrieved in async mode
            auto nextImage{vkSwapchain->acquireNextImage(timeout, {}, *fence)};
            if (nextImage.first == vk::Result::eSuccess) {
                swapchain.dequeuedCount++;
                while (gpu.vkDevice.waitForFences(*fence, true, std::numeric_limits<u64>::max()) == vk::Result::eTimeout);
                slot = nextImage.second;
                return AndroidStatus::Ok;
            } else if (nextImage.first == vk::Result::eNotReady || nextImage.first == vk::Result::eTimeout) {
                return AndroidStatus::WouldBlock;
            } else if (nextImage.first == vk::Result::eSuboptimalKHR) {
                surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
                return GetFreeTexture(async, slot);
            } else {
                throw exception("VkAcquireNextImageKHR returned an unhandled result '{}'", vk::to_string(nextImage.first));
            }
        }
        return AndroidStatus::Busy;
    }

    void PresentationEngine::Present(u32 slot) {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });

        if (--swapchain.dequeuedCount < 0) [[unlikely]] {
            throw exception("Swapchain has been presented more times than images from it have been acquired: {} (Image Count: {})", swapchain.dequeuedCount, swapchain.imageCount);
        }

        {
            std::lock_guard queueLock(gpu.queueMutex);
            static_cast<void>(gpu.vkQueue.presentKHR(vk::PresentInfoKHR{
                .swapchainCount = 1,
                .pSwapchains = &**vkSwapchain,
                .pImageIndices = &slot,
            })); // We explicitly discard the result here as suboptimal images are expected when the game doesn't respect the transform hint
        }

        vsyncEvent->Signal();

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
