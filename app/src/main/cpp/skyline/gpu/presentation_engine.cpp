// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window_jni.h>
#include <gpu.h>
#include "jvm.h"
#include "presentation_engine.h"

extern skyline::i32 Fps;
extern skyline::i32 FrameTime;

namespace skyline::gpu {
    PresentationEngine::PresentationEngine(const DeviceState &state, const GPU &gpu) : state(state), gpu(gpu), vsyncEvent(std::make_shared<kernel::type::KEvent>(state, true)), bufferEvent(std::make_shared<kernel::type::KEvent>(state, true)), presentationTrack(static_cast<u64>(trace::TrackIds::Presentation), perfetto::ProcessTrack::Current()) {
        auto desc{presentationTrack.Serialize()};
        desc.set_name("Presentation");
        perfetto::TrackEvent::SetTrackDescriptor(presentationTrack, desc);
    }

    PresentationEngine::~PresentationEngine() {
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(surface, nullptr))
            env->DeleteGlobalRef(surface);
    }

    void PresentationEngine::UpdateSwapchain(u32 imageCount, vk::Format imageFormat, vk::Extent2D imageExtent) {
        if (!imageCount)
            return;

        auto capabilities{gpu.vkPhysicalDevice.getSurfaceCapabilitiesKHR(**vkSurface)};
        if (imageCount < capabilities.minImageCount || (capabilities.maxImageCount && imageCount > capabilities.maxImageCount))
            throw exception("Cannot update swapchain to accomodate image count: {} ({}-{})", imageCount, capabilities.minImageCount, capabilities.maxImageCount);
        if (capabilities.minImageExtent.height > imageExtent.height || capabilities.minImageExtent.width > imageExtent.width || capabilities.maxImageExtent.height < imageExtent.height || capabilities.maxImageExtent.width < imageExtent.width)
            throw exception("Cannot update swapchain to accomodate image extent: {}x{} ({}x{}-{}x{})", imageExtent.width, imageExtent.height, capabilities.minImageExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);

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

        swapchain = SwapchainContext{
            .imageCount = imageCount,
            .imageFormat = imageFormat,
            .imageExtent = imageExtent,
        };
    }

    void PresentationEngine::UpdateSurface(jobject newSurface) {
        std::lock_guard lock(mutex);

        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(surface, nullptr)) {
            env->DeleteGlobalRef(surface);
            surface = nullptr;
        }
        if (!env->IsSameObject(newSurface, nullptr))
            surface = env->NewGlobalRef(newSurface);

        if (surface) {
            vkSurface.emplace(gpu.vkInstance, vk::AndroidSurfaceCreateInfoKHR{
                .window = ANativeWindow_fromSurface(env, surface),
            });
            if (!gpu.vkPhysicalDevice.getSurfaceSupportKHR(gpu.vkQueueFamilyIndex, **vkSurface))
                throw exception("Vulkan Queue doesn't support presentation with surface");

            UpdateSwapchain(swapchain.imageCount, swapchain.imageFormat, swapchain.imageExtent);

            surfaceCondition.notify_all();
        } else {
            vkSurface.reset();
        }
    }

    std::shared_ptr<Texture> PresentationEngine::CreatePresentationTexture(const std::shared_ptr<GuestTexture> &texture, u32 slot) {
        std::unique_lock lock(mutex);
        if (swapchain.imageCount <= slot)
            UpdateSwapchain(slot + 1, texture->format.vkFormat, texture->dimensions);
        return texture->InitializeTexture(vk::raii::Image(gpu.vkDevice, vkSwapchain->getImages().at(slot)));
    }

    u32 PresentationEngine::GetFreeTexture() {
        std::unique_lock lock(mutex);
        auto nextImage{vkSwapchain->acquireNextImage(std::numeric_limits<u64>::max())};
        if (nextImage.first == vk::Result::eErrorSurfaceLostKHR || nextImage.first == vk::Result::eSuboptimalKHR) {
            surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
            return GetFreeTexture();
        }
        return nextImage.second;
    }

    void PresentationEngine::Present(const std::shared_ptr<Texture> &texture) {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });

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
}
