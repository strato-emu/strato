// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window_jni.h>
#include "jvm.h"
#include "presentation_engine.h"

extern skyline::u16 fps;
extern skyline::u32 frametime;

namespace skyline::gpu {
    PresentationEngine::PresentationEngine(const DeviceState &state) : state(state), vsyncEvent(std::make_shared<kernel::type::KEvent>(state)), bufferEvent(std::make_shared<kernel::type::KEvent>(state)) {}

    PresentationEngine::~PresentationEngine() {
        if (window)
            ANativeWindow_release(window);
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(surface, nullptr))
            env->DeleteGlobalRef(surface);
    }

    void PresentationEngine::UpdateSurface(jobject newSurface) {
        std::lock_guard lock(windowMutex);
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(surface, nullptr)) {
            env->DeleteGlobalRef(surface);
            surface = nullptr;
        }
        if (!env->IsSameObject(newSurface, nullptr))
            surface = env->NewGlobalRef(newSurface);
        if (surface) {
            window = ANativeWindow_fromSurface(state.jvm->GetEnv(), surface);
            ANativeWindow_acquire(window);
            resolution.width = static_cast<u32>(ANativeWindow_getWidth(window));
            resolution.height = static_cast<u32>(ANativeWindow_getHeight(window));
            format = ANativeWindow_getFormat(window);
            windowConditional.notify_all();
        } else {
            window = nullptr;
        }
    }

    void PresentationEngine::Present(const std::shared_ptr<Texture> &texture) {
        std::unique_lock lock(windowMutex);
        if (!window)
            windowConditional.wait(lock, [this]() { return window; });

        auto textureFormat{[&texture]() {
            switch (texture->format.vkFormat) {
                case vk::Format::eR8G8B8A8Unorm:
                    return WINDOW_FORMAT_RGBA_8888;
                case vk::Format::eR5G6B5UnormPack16:
                    return WINDOW_FORMAT_RGB_565;
                default:
                    throw exception("Cannot find corresponding Android surface format");
            }
        }()};
        if (resolution != texture->dimensions || textureFormat != format) {
            ANativeWindow_setBuffersGeometry(window, texture->dimensions.width, texture->dimensions.height, textureFormat);
            resolution = texture->dimensions;
            format = textureFormat;
        }

        ANativeWindow_Buffer buffer;
        ARect rect;

        ANativeWindow_lock(window, &buffer, &rect);
        std::memcpy(buffer.bits, texture->backing.data(), texture->backing.size());
        ANativeWindow_unlockAndPost(window);

        vsyncEvent->Signal();

        if (frameTimestamp) {
            auto now{util::GetTimeNs()};

            frametime = static_cast<u32>((now - frameTimestamp) / 10000); // frametime / 100 is the real ms value, this is to retain the first two decimals
            fps = static_cast<u16>(constant::NsInSecond / (now - frameTimestamp));

            frameTimestamp = now;
        } else {
            frameTimestamp = util::GetTimeNs();
        }
    }
}
