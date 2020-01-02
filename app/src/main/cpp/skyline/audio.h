#pragma once

#include <queue>
#include <oboe/Oboe.h>

#include <kernel/types/KEvent.h>
#include <audio/track.h>
#include "common.h"

namespace skyline::audio {
    /**
     * @brief The Audio class is used to mix audio from all tracks
     */
    class Audio : public oboe::AudioStreamCallback {
      private:
        const DeviceState &state; //!< The state of the device
        oboe::ManagedStream outputStream; //!< The output oboe audio stream
        std::vector<std::shared_ptr<audio::AudioTrack>> audioTracks; //!< Vector containing a pointer of every open audio track

      public:
        Audio(const DeviceState &state);

        /**
         * @brief Opens a new track that can be used to play sound
         * @param channelCount The amount channels that are present in the track
         * @param sampleRate The sample rate of the track
         * @param releaseCallback The callback to call when a buffer has been released
         * @return A shared pointer to a new AudioTrack object
         */
        std::shared_ptr<AudioTrack> OpenTrack(int channelCount, int sampleRate, const std::function<void()> &releaseCallback);

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
    };
}
