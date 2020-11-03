// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <oboe/Oboe.h>
#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u16 SampleRate{48000}; //!< The common sampling rate to use for audio output
        constexpr u8 ChannelCount{2}; //!< The common amount of channels to use for audio output
        constexpr u16 MixBufferSize{960}; //!< The size of the mix buffer by default
        constexpr auto PcmFormat{oboe::AudioFormat::I16}; //!< The common PCM data format to use for audio output
    };

    namespace audio {
        enum class AudioFormat : u8 {
            Invalid = 0, //!< An invalid PCM format
            Int8 = 1,    //!< 8 bit integer PCM
            Int16 = 2,   //!< 16 bit integer PCM
            Int24 = 3,   //!< 24 bit integer PCM
            Int32 = 4,   //!< 32 bit integer PCM
            Float = 5,   //!< Floating point PCM
            ADPCM = 6,   //!< Adaptive differential PCM
        };

        enum class AudioOutState : u8 {
            Started = 0, //!< Stream is started and is playing
            Stopped = 1, //!< Stream is stopped, there are no samples left to play
            Paused = 2,  //!< Stream is paused, some samples may not have been played yet
        };

        struct BufferIdentifier {
            u64 tag;
            u64 finalSample; //!< The final sample this buffer will be played in, after that the buffer can be safely released
            bool released; //!< If the buffer has been released (fully played back)
        };

        /**
         * @brief Saturates the specified value according to the numeric limits of Out
         * @tparam Out The return value type and the numeric limit clamp
         * @tparam Intermediate The intermediate type that is converted to from In before clamping
         */
        template<typename Out, typename Intermediate, typename In>
        inline Out Saturate(In value) {
            return static_cast<Out>(std::clamp(static_cast<Intermediate>(value), static_cast<Intermediate>(std::numeric_limits<Out>::min()), static_cast<Intermediate>(std::numeric_limits<Out>::max())));
        }
    }
}
