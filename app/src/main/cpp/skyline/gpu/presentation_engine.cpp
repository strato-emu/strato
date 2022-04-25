// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window_jni.h>
#include <android/choreographer.h>
#include <common/settings.h>
#include <common/signal.h>
#include <jvm.h>
#include <gpu.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include "presentation_engine.h"
#include "native_window.h"
#include "texture/format.h"

extern jint Fps;
extern jfloat AverageFrametimeMs;
extern jfloat AverageFrametimeDeviationMs;

namespace skyline::gpu {
    using namespace service::hosbinder;

    PresentationEngine::PresentationEngine(const DeviceState &state, GPU &gpu)
        : state(state),
          gpu(gpu),
          acquireFence(gpu.vkDevice, vk::FenceCreateInfo{}),
          presentationTrack(static_cast<u64>(trace::TrackIds::Presentation), perfetto::ProcessTrack::Current()),
          choreographerThread(&PresentationEngine::ChoreographerThread, this),
          vsyncEvent(std::make_shared<kernel::type::KEvent>(state, true)) {
        auto desc{presentationTrack.Serialize()};
        desc.set_name("Presentation");
        perfetto::TrackEvent::SetTrackDescriptor(presentationTrack, desc);
    }

    PresentationEngine::~PresentationEngine() {
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr))
            env->DeleteGlobalRef(jSurface);

        if (choreographerThread.joinable()) {
            if (choreographerLooper) {
                choreographerStop = true;
                ALooper_wake(choreographerLooper);
            }
            choreographerThread.join();
        }
    }

    void PresentationEngine::ChoreographerCallback(int64_t frameTimeNanos, PresentationEngine *engine) {
        // If the duration of this cycle deviates by ±0.5ms from the current refresh cycle duration then we reevaluate it
        i64 cycleLength{frameTimeNanos - engine->lastChoreographerTime};
        if (std::abs(cycleLength - engine->refreshCycleDuration) > (constant::NsInMillisecond / 2)) {
            if (engine->window)
                engine->window->perform(engine->window, NATIVE_WINDOW_GET_REFRESH_CYCLE_DURATION, &engine->refreshCycleDuration);
            else
                engine->refreshCycleDuration = cycleLength;
        }

        // Record the current cycle's timestamp and signal the V-Sync event to notify the game that a frame has been displayed
        engine->lastChoreographerTime = frameTimeNanos;
        engine->vsyncEvent->Signal();

        // Post the frame callback to be triggered on the next display refresh
        AChoreographer_postFrameCallback64(AChoreographer_getInstance(), reinterpret_cast<AChoreographer_frameCallback64>(&ChoreographerCallback), engine);
    }

    void PresentationEngine::ChoreographerThread() {
        pthread_setname_np(pthread_self(), "Skyline-Choreographer");
        try {
            signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, signal::ExceptionalSignalHandler);
            choreographerLooper = ALooper_prepare(0);
            AChoreographer_postFrameCallback64(AChoreographer_getInstance(), reinterpret_cast<AChoreographer_frameCallback64>(&ChoreographerCallback), this);
            while (ALooper_pollAll(-1, nullptr, nullptr, nullptr) == ALOOPER_POLL_WAKE && !choreographerStop); // Will block and process callbacks till ALooper_wake() is called with choreographerStop set
        } catch (const signal::SignalException &e) {
            Logger::Error("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        } catch (const std::exception &e) {
            Logger::Error(e.what());
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        }
    }

    NativeWindowTransform GetAndroidTransform(vk::SurfaceTransformFlagBitsKHR transform) {
        using NativeWindowTransform = NativeWindowTransform;
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

        vk::Format vkFormat{*format};
        if (format == gpu::format::R5G6B5Unorm)
            // B5G6R5 isn't generally supported by the swapchain and the format is used for R5G6B5 with swapped R/B channels to avoid aliasing so we reverse that by using R5G6B5 as the underlying Vulkan format for the swapchain which should be automatically handled by the driver for any copies from B5G6R5 textures and the data representation should be the same as B5G6R5 with swapped R/B channels so not reporting the correct texture::Format should be fine
            vkFormat = vk::Format::eR5G6B5UnormPack16;

        if (swapchainFormat != format) {
            auto formats{gpu.vkPhysicalDevice.getSurfaceFormatsKHR(**vkSurface)};
            if (std::find(formats.begin(), formats.end(), vk::SurfaceFormatKHR{vkFormat, vk::ColorSpaceKHR::eSrgbNonlinear}) == formats.end())
                Logger::Warn("Surface doesn't support requested image format '{}' with colorspace '{}'", vk::to_string(vkFormat), vk::to_string(vk::ColorSpaceKHR::eSrgbNonlinear));
        }

        constexpr vk::ImageUsageFlags presentUsage{vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst};
        if ((capabilities.supportedUsageFlags & presentUsage) != presentUsage)
            throw exception("Swapchain doesn't support image usage '{}': {}", vk::to_string(presentUsage), vk::to_string(capabilities.supportedUsageFlags));

        auto requestedMode{state.settings->disableFrameThrottling ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo};
        auto modes{gpu.vkPhysicalDevice.getSurfacePresentModesKHR(**vkSurface)};
        if (std::find(modes.begin(), modes.end(), requestedMode) == modes.end())
            throw exception("Swapchain doesn't support present mode: {}", vk::to_string(requestedMode));

        vkSwapchain.emplace(gpu.vkDevice, vk::SwapchainCreateInfoKHR{
            .surface = **vkSurface,
            .minImageCount = minImageCount,
            .imageFormat = vkFormat,
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = presentUsage,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eInherit,
            .presentMode = requestedMode,
            .clipped = true,
        });

        auto vkImages{vkSwapchain->getImages()};
        if (vkImages.size() > MaxSwapchainImageCount)
            throw exception("Swapchain has higher image count ({}) than maximum slot count ({})", minImageCount, MaxSwapchainImageCount);

        for (size_t index{}; index < vkImages.size(); index++) {
            auto &slot{images[index]};
            slot = std::make_shared<Texture>(*state.gpu, vkImages[index], extent, format, vk::ImageLayout::eUndefined, vk::ImageTiling::eOptimal);
            slot->TransitionLayout(vk::ImageLayout::ePresentSrcKHR);
        }
        for (size_t index{vkImages.size()}; index < MaxSwapchainImageCount; index++)
            // We need to clear all the slots which aren't filled, keeping around stale slots could lead to issues
            images[index] = {};

        swapchainFormat = format;
        swapchainExtent = extent;
    }

    void PresentationEngine::UpdateSurface(jobject newSurface) {
        std::scoped_lock guard{mutex};

        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr)) {
            env->DeleteGlobalRef(jSurface);
            jSurface = nullptr;
        }
        if (!env->IsSameObject(newSurface, nullptr))
            jSurface = env->NewGlobalRef(newSurface);

        vkSwapchain.reset();

        if (jSurface) {
            window = ANativeWindow_fromSurface(env, jSurface);
            vkSurface.emplace(gpu.vkInstance, vk::AndroidSurfaceCreateInfoKHR{
                .window = window,
            });
            if (!gpu.vkPhysicalDevice.getSurfaceSupportKHR(gpu.vkQueueFamilyIndex, **vkSurface))
                throw exception("Vulkan Queue doesn't support presentation with surface");
            vkSurfaceCapabilities = gpu.vkPhysicalDevice.getSurfaceCapabilitiesKHR(**vkSurface);

            if (swapchainExtent && swapchainFormat)
                UpdateSwapchain(swapchainFormat, swapchainExtent);

            if (window->common.magic != AndroidNativeWindowMagic)
                throw exception("ANativeWindow* has unexpected magic: {} instead of {}", span(&window->common.magic, 1).as_string(true), span<const u8>(reinterpret_cast<const u8 *>(&AndroidNativeWindowMagic), sizeof(u32)).as_string(true));
            if (window->common.version != sizeof(ANativeWindow))
                throw exception("ANativeWindow* has unexpected version: {} instead of {}", window->common.version, sizeof(ANativeWindow));

            int result;
            if (windowCrop && (result = window->perform(window, NATIVE_WINDOW_SET_CROP, &windowCrop)))
                throw exception("Setting the layer crop to ({}-{})x({}-{}) failed with {}", windowCrop.left, windowCrop.right, windowCrop.top, windowCrop.bottom, result);

            if (windowScalingMode != NativeWindowScalingMode::ScaleToWindow && (result = window->perform(window, NATIVE_WINDOW_SET_SCALING_MODE, static_cast<i32>(windowScalingMode))))
                throw exception("Setting the layer scaling mode to '{}' failed with {}", ToString(windowScalingMode), result);

            if (windowTransform != NativeWindowTransform::Identity && (result = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_TRANSFORM, static_cast<i32>(windowTransform))))
                throw exception("Setting the buffer transform to '{}' failed with {}", ToString(windowTransform), result);

            if ((result = window->perform(window, NATIVE_WINDOW_ENABLE_FRAME_TIMESTAMPS, true)))
                throw exception("Enabling frame timestamps failed with {}", result);

            surfaceCondition.notify_all();
        } else {
            vkSurface.reset();
            window = nullptr;
        }
    }

    void PresentationEngine::Present(const std::shared_ptr<Texture> &texture, i64 timestamp, u64 swapInterval, AndroidRect crop, NativeWindowScalingMode scalingMode, NativeWindowTransform transform, u64 &frameId) {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });

        if (texture->format != swapchainFormat || texture->dimensions != swapchainExtent)
            UpdateSwapchain(texture->format, texture->dimensions);

        int result;
        if (crop && crop != windowCrop) {
            if ((result = window->perform(window, NATIVE_WINDOW_SET_CROP, &crop)))
                throw exception("Setting the layer crop to ({}-{})x({}-{}) failed with {}", crop.left, crop.right, crop.top, crop.bottom, result);
            windowCrop = crop;
        }

        if (scalingMode != NativeWindowScalingMode::Freeze && windowScalingMode != scalingMode) {
            if ((result = window->perform(window, NATIVE_WINDOW_SET_SCALING_MODE, static_cast<i32>(scalingMode))))
                throw exception("Setting the layer scaling mode to '{}' failed with {}", ToString(scalingMode), result);
            windowScalingMode = scalingMode;
        }

        if (transform != windowTransform) {
            if ((result = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_TRANSFORM, static_cast<i32>(transform))))
                throw exception("Setting the buffer transform to '{}' failed with {}", ToString(transform), result);
            windowTransform = transform;
        }

        gpu.vkDevice.resetFences(*acquireFence);

        std::pair<vk::Result, u32> nextImage;
        while (nextImage = vkSwapchain->acquireNextImage(std::numeric_limits<u64>::max(), {}, *acquireFence), nextImage.first != vk::Result::eSuccess) [[unlikely]] {
            if (nextImage.first == vk::Result::eSuboptimalKHR)
                surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
            else
                throw exception("vkAcquireNextImageKHR returned an unhandled result '{}'", vk::to_string(nextImage.first));
        }

        std::ignore = gpu.vkDevice.waitForFences(*acquireFence, true, std::numeric_limits<u64>::max());
        images.at(nextImage.second)->CopyFrom(texture, vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1,
        });

        if (timestamp) {
            // If the timestamp is specified, we need to convert it from the util::GetTimeNs base to the CLOCK_MONOTONIC one
            // We do so by getting an offset from the current time in nanoseconds and then adding it to the current time in CLOCK_MONOTONIC
            // Note: It's important we do this right before present as going past the timestamp could lead to fewer Binder IPC calls
            i64 current{util::GetTimeNs()};
            if (current < timestamp) {
                timespec time;
                if (clock_gettime(CLOCK_MONOTONIC, &time))
                    throw exception("Failed to clock_gettime with '{}'", strerror(errno));
                timestamp = ((time.tv_sec * constant::NsInSecond) + time.tv_nsec) + (timestamp - current);
            } else {
                timestamp = 0;
            }
        }

        if (swapInterval > 1)
            // If we have a swap interval above 1 we have to adjust the timestamp to emulate the swap interval
            timestamp = std::max(timestamp, lastChoreographerTime + (refreshCycleDuration * static_cast<i64>(swapInterval) * 2));

        auto lastTimestamp{std::exchange(windowLastTimestamp, timestamp)};
        if (!timestamp && lastTimestamp)
            // We need to nullify the timestamp if it transitioned from being specified (non-zero) to unspecified (zero)
            timestamp = NativeWindowTimestampAuto;

        if (timestamp && (result = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP, timestamp)))
            throw exception("Setting the buffer timestamp to {} failed with {}", timestamp, result);

        if ((result = window->perform(window, NATIVE_WINDOW_GET_NEXT_FRAME_ID, &frameId)))
            throw exception("Retrieving the next frame's ID failed with {}", result);

        {
            std::scoped_lock queueLock{gpu.queueMutex};
            std::ignore = gpu.vkQueue.presentKHR(vk::PresentInfoKHR{
                .swapchainCount = 1,
                .pSwapchains = &**vkSwapchain,
                .pImageIndices = &nextImage.second,
            }); // We don't care about suboptimal images as they are caused by not respecting the transform hint, we handle transformations externally
        }

        if (frameTimestamp) {
            i64 now{util::GetTimeNs()};
            i64 sampleWeight{swapInterval ? constant::NsInSecond / (refreshCycleDuration * static_cast<i64>(swapInterval)) : 10}; //!< The weight of each sample in calculating the average, we arbitrarily average 10 samples for unlocked FPS

            auto weightedAverage{[](auto weight, auto previousAverage, auto current) {
                return (((weight - 1) * previousAverage) + current) / weight;
            }}; //!< Modified moving average (https://en.wikipedia.org/wiki/Moving_average#Modified_moving_average)

            i64 currentFrametime{now - frameTimestamp};
            averageFrametimeNs = weightedAverage(sampleWeight, averageFrametimeNs, currentFrametime);
            AverageFrametimeMs = static_cast<jfloat>(averageFrametimeNs) / constant::NsInMillisecond;

            i64 currentFrametimeDeviation{std::abs(averageFrametimeNs - currentFrametime)};
            averageFrametimeDeviationNs = weightedAverage(sampleWeight, averageFrametimeDeviationNs, currentFrametimeDeviation);
            AverageFrametimeDeviationMs = static_cast<jfloat>(averageFrametimeDeviationNs) / constant::NsInMillisecond;

            Fps = static_cast<jint>(std::round(static_cast<float>(constant::NsInSecond) / static_cast<float>(averageFrametimeNs)));

            TRACE_EVENT_INSTANT("gpu", "Present", presentationTrack, "FrameTimeNs", now - frameTimestamp, "Fps", Fps);

            frameTimestamp = now;
        } else {
            frameTimestamp = util::GetTimeNs();
        }
    }

    NativeWindowTransform PresentationEngine::GetTransformHint() {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
        return GetAndroidTransform(vkSurfaceCapabilities.currentTransform);
    }
}
