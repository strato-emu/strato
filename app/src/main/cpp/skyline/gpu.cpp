// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "gpu.h"
#include "jvm.h"
#include <kernel/types/KProcess.h>
#include <android/native_window_jni.h>

extern bool Halt;
extern jobject Surface;

namespace skyline::gpu {
    GPU::GPU(const DeviceState &state) : state(state), window(ANativeWindow_fromSurface(state.jvm->GetEnv(), Surface)), vsyncEvent(std::make_shared<kernel::type::KEvent>(state)), bufferEvent(std::make_shared<kernel::type::KEvent>(state)) {
        ANativeWindow_acquire(window);
        resolution.width = static_cast<u32>(ANativeWindow_getWidth(window));
        resolution.height = static_cast<u32>(ANativeWindow_getHeight(window));
        format = ANativeWindow_getFormat(window);
    }

    GPU::~GPU() {
        ANativeWindow_release(window);
    }

    void GPU::Loop() {
        if (surfaceUpdate) {
            if (Surface == nullptr)
                return;
            window = ANativeWindow_fromSurface(state.jvm->GetEnv(), Surface);
            ANativeWindow_acquire(window);
            resolution.width = static_cast<u32>(ANativeWindow_getWidth(window));
            resolution.height = static_cast<u32>(ANativeWindow_getHeight(window));
            format = ANativeWindow_getFormat(window);
            surfaceUpdate = false;
        } else if (Surface == nullptr) {
            surfaceUpdate = true;
            return;
        }

        if (!presentationQueue.empty()) {
            auto &texture = presentationQueue.front();
            presentationQueue.pop();

            auto textureFormat = texture->GetAndroidFormat();
            if (resolution != texture->dimensions || textureFormat != format) {
                ANativeWindow_setBuffersGeometry(window, texture->dimensions.width, texture->dimensions.height, textureFormat);
                resolution = texture->dimensions;
                format = textureFormat;
            }

            ANativeWindow_Buffer windowBuffer;
            ARect rect;

            ANativeWindow_lock(window, &windowBuffer, &rect);
            std::memcpy(windowBuffer.bits, texture->backing.data(), texture->backing.size());
            ANativeWindow_unlockAndPost(window);

            vsyncEvent->Signal();
            texture->releaseCallback();

            if (prevTime != 0) {
                auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                state.logger->Error("{} ms, {} FPS", (now - prevTime) / 1000, 1000000 / (now - prevTime));
            }

            prevTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }
}
