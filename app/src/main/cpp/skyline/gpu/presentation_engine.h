// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/trace.h>
#include <kernel/types/KEvent.h>
#include "texture.h"

struct ANativeWindow;

namespace skyline::gpu {
    class PresentationEngine {
      private:
        const DeviceState &state;
        std::mutex windowMutex;
        std::condition_variable windowConditional;
        jobject surface{}; //!< The Surface object backing the ANativeWindow
        u64 frameTimestamp{}; //!< The timestamp of the last frame being shown
        perfetto::Track presentationTrack; //!< Perfetto track used for presentation events

      public:
        texture::Dimensions resolution{};
        i32 format{};
        std::shared_ptr<kernel::type::KEvent> vsyncEvent; //!< Signalled every time a frame is drawn
        std::shared_ptr<kernel::type::KEvent> bufferEvent; //!< Signalled every time a buffer is freed

        PresentationEngine(const DeviceState &state);

        ~PresentationEngine();

        void UpdateSurface(jobject newSurface);

        void Present(const std::shared_ptr<Texture> &texture);

        ANativeWindow *window{};
    };
}
