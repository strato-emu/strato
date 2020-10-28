// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include "common.h"

namespace skyline::audio {
    /**
     * @brief The AudioTrack class manages the buffers for an audio stream
     */
    class AudioTrack {
      private:
        std::function<void()> releaseCallback; //!< Callback called when a buffer has been played
        std::deque<BufferIdentifier> identifiers; //!< Queue of all appended buffer identifiers

        u8 channelCount;
        u32 sampleRate;

      public:
        CircularBuffer<i16, constant::SampleRate * constant::ChannelCount * 10> samples; //!< A circular buffer with all appended audio samples
        std::mutex bufferLock; //!< Synchronizes appending to audio buffers

        AudioOutState playbackState{AudioOutState::Stopped}; //!< The current state of playback
        u64 sampleCounter{}; //!< A counter used for tracking when buffers have been played and can be released

        /**
         * @param channelCount The amount channels that will be present in the track
         * @param sampleRate The sample rate to use for the track
         * @param releaseCallback A callback to call when a buffer has been played
         */
        AudioTrack(u8 channelCount, u32 sampleRate, const std::function<void()> &releaseCallback);

        /**
         * @brief Starts audio playback using data from appended buffers
         */
        inline void Start() {
            playbackState = AudioOutState::Started;
        }

        /**
         * @brief Stops audio playback, this waits for audio playback to finish before returning
         */
        void Stop();

        /**
         * @brief Checks if a buffer has been released
         * @param tag The tag of the buffer to check
         * @return True if the given buffer hasn't been released
         */
        bool ContainsBuffer(u64 tag);

        /**
         * @brief Gets the IDs of all newly released buffers
         * @param max The maximum amount of buffers to return
         * @return A vector containing the identifiers of the buffers
         */
        std::vector<u64> GetReleasedBuffers(u32 max);

        /**
         * @brief Appends audio samples to the output buffer
         * @param tag The tag of the buffer
         * @param buffer A span containing the source sample buffer
         */
        void AppendBuffer(u64 tag, span<i16> buffer = {});

        /**
         * @brief Checks if any buffers have been released and calls the appropriate callback for them
         * @note bufferLock MUST be locked when calling this
         */
        void CheckReleasedBuffers();
    };
}
