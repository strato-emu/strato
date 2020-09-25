// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <audio/track.h>

namespace skyline::audio {
    /**
     * @brief The Audio class is used to mix audio from all tracks
     */
    class Audio : public oboe::AudioStreamCallback {
      private:
        oboe::AudioStreamBuilder builder; //!< The audio stream builder, used to open
        oboe::ManagedStream outputStream; //!< The output oboe audio stream
        std::vector<std::shared_ptr<AudioTrack>> audioTracks; //!< A vector of shared_ptr to every open audio track
        Mutex trackLock; //!< This mutex is used to ensure that audioTracks isn't modified while it is being used

      public:
        Audio(const DeviceState &state);

        /**
         * @brief Opens a new track that can be used to play sound
         * @param channelCount The amount channels that are present in the track
         * @param sampleRate The sample rate of the track
         * @param releaseCallback The callback to call when a buffer has been released
         * @return A shared pointer to a new AudioTrack object
         */
        std::shared_ptr<AudioTrack> OpenTrack(u8 channelCount, u32 sampleRate, const std::function<void()> &releaseCallback);

        /**
         * @brief Closes a track and frees its data
         * @param track The track to close
         */
        void CloseTrack(std::shared_ptr<AudioTrack> &track);

        /**
         * @brief The callback oboe uses to get audio sample data
         * @param audioStream The audio stream we are being called by
         * @param audioData The raw audio sample data
         * @param numFrames The amount of frames the sample data needs to contain
         */
        oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames);

        /**
         * @brief The callback oboe uses to notify the application about stream closure
         * @param audioStream The audio stream we are being called by
         * @param error The error due to which the stream is being closed
         */
        void onErrorAfterClose(oboe::AudioStream *audioStream, oboe::Result error);
    };
}
