// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <audio_core/core/core.h>
#include <common.h>

namespace AudioCore::AudioOut {
    class Manager;
}

namespace AudioCore::AudioRenderer {
    class Manager;
}

namespace skyline::audio {
    /**
     * @brief The Audio class is used to bridge yuzu's audio core with services
     */
    class Audio {
      public:
        Core::System audioSystem{};
        std::unique_ptr<AudioCore::AudioOut::Manager> audioOutManager;
        std::unique_ptr<AudioCore::AudioRenderer::Manager> audioRendererManager;

        Audio(const DeviceState &state);

        ~Audio();

        void Pause();

        void Resume();
    };
}
