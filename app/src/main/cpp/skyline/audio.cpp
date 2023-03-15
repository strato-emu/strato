// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <audio_core/audio_core.h>
#include <audio_core/audio_out_manager.h>
#include <audio_core/common/settings.h>
#include <common/settings.h>
#include <common/utils.h>
#include "audio.h"

namespace AudioCore::Log {
    void Debug(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Debug, message);
    }

    void Info(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Info, message);
    }

    void Warn(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Warn, message);
    }

    void Error(const std::string &message) {
        skyline::Logger::Write(skyline::Logger::LogLevel::Error, message);
    }
}

namespace Core::Timing {
    skyline::u64 GetClockTicks() {
        return skyline::util::GetTimeTicks();
    }

    std::chrono::nanoseconds GetClockNs() {
        return std::chrono::nanoseconds{skyline::util::GetTimeNs()};
    }
}

namespace skyline::audio {
    Audio::Audio(const DeviceState &state)
        : audioOutManager{std::make_unique<AudioCore::AudioOut::Manager>(audioSystem)},
          audioRendererManager{std::make_unique<AudioCore::AudioRenderer::Manager>(audioSystem)} {
        AudioCore::Settings::values.volume  = *state.settings->isAudioOutputDisabled ? 0 : 200;
    }

    Audio::~Audio() = default;

    void Audio::Pause() {
        audioSystem.AudioCore().GetOutputSink().SetSystemVolume(0.0f);
    }

    void Audio::Resume() {
        audioSystem.AudioCore().GetOutputSink().SetSystemVolume(1.0f);
    }
}
