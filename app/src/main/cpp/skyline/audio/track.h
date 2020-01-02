#pragma once

#include <queue>

#include <kernel/types/KEvent.h>
#include <common.h>
#include "common.h"

namespace skyline::audio {
    /**
     * @brief The AudioTrack class manages the buffers for an audio stream
     */
    class AudioTrack {
      private:
        const std::function<void()> releaseCallback; //!< Callback called when a buffer has been played
        std::deque<BufferIdentifier> identifierQueue; //!< Queue of all appended buffer identifiers

        int channelCount; //!< The amount channels present in the track
        int sampleRate; //!< The sample rate of the track

      public:
        std::queue<i16> sampleQueue; //!< Queue of all appended buffer data
        skyline::Mutex bufferLock; //!< Buffer access lock

        AudioOutState playbackState{AudioOutState::Stopped}; //!< The current state of playback
        u64 sampleCounter{}; //!< A counter used for tracking buffer status

        /**
         * @param channelCount The amount channels that will be present in the track
         * @param sampleRate The sample rate to use for the track
         * @param releaseCallback A callback to call when a buffer has been played
         */
        AudioTrack(int channelCount, int sampleRate, const std::function<void()> &releaseCallback);

        /**
         * @brief Starts audio playback using data from appended buffers.
         */
        inline void Start() {
            playbackState = AudioOutState::Started;
        }

        /**
         * @brief Stops audio playback. This waits for audio playback to finish before returning.
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
         * @param sampleData Reference to a vector containing I16 format pcm data
         * @param tag The tag of the buffer
         */
        void AppendBuffer(const std::vector<i16> &sampleData, u64 tag);

        /**
         * @brief Checks if any buffers have been released and calls the appropriate callback for them
         */
        void CheckReleasedBuffers();
    };
}
