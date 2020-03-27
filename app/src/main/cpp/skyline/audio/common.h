// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <oboe/Oboe.h>
#include <common.h>

namespace skyline {
    namespace constant {
        constexpr auto SampleRate = 48000; //!< The sampling rate to use for the oboe audio output
        constexpr auto ChannelCount = 2; //!< The amount of channels to use for the oboe audio output
        constexpr auto PcmFormat = oboe::AudioFormat::I16; //!< The pcm data format to use for the oboe audio output
    };

    namespace audio {
        /**
         * @brief The available PCM stream formats
         */
        enum class PcmFormat : u8 {
            Invalid = 0, //!< An invalid PCM format
            Int8 = 1, //!< 8 bit integer PCM
            Int16 = 2, //!< 16 bit integer PCM
            Int24 = 3, //!< 24 bit integer PCM
            Int32 = 4, //!< 32 bit integer PCM
            PcmFloat = 5, //!< Floating point PCM
            AdPcm = 6 //!< Adaptive differential PCM
        };

        /**
         * @brief The state of an audio track
         */
        enum class AudioOutState : u8 {
            Started = 0, //!< Stream is started and is playing
            Stopped = 1, //!< Stream is stopped, there are no samples left to play
            Paused = 2 //!< Stream is paused, some samples may not have been played yet
        };

        /**
         * @brief Stores various information about pushed buffers
         */
        struct BufferIdentifier {
            u64 tag; //!< The tag of the buffer
            u64 finalSample; //!< The final sample this buffer will be played in
            bool released; //!< Whether the buffer has been released
        };
    }
}
