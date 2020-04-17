// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>
#include <services/serviceman.h>
#include <audio/resampler.h>
#include <audio.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioOut is a service opened when OpenAudioOut is called by IAudioOutManager (https://switchbrew.org/wiki/Audio_services#IAudioOut)
     */
    class IAudioOut : public BaseService {
      private:
        skyline::audio::Resampler resampler; //!< The audio resampler object used to resample audio
        std::shared_ptr<skyline::audio::AudioTrack> track; //!< The audio track associated with the audio out
        std::shared_ptr<type::KEvent> releaseEvent; //!< The KEvent that is signalled when a buffer has been released
        std::vector<i16> tmpSampleBuffer; //!< A temporary buffer used to store sample data in AppendAudioOutBuffer

        const u32 sampleRate; //!< The sample rate of the audio out
        const u8 channelCount; //!< The amount of channels in the data sent to the audio out

      public:
        /**
         * @param channelCount The channel count of the audio data the audio out will be fed
         * @param sampleRate The sample rate of the audio data the audio out will be fed
         */
        IAudioOut(const DeviceState &state, ServiceManager &manager, const u8 channelCount, const u32 sampleRate);

        /**
         * @brief Closes the audio track
         */
        ~IAudioOut();

        /**
         * @brief Returns the playback state of the audio output (https://switchbrew.org/wiki/Audio_services#GetAudioOutState)
         */
        void GetAudioOutState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Starts playback using data from appended samples (https://switchbrew.org/wiki/Audio_services#StartAudioOut)
        */
        void StartAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Stops playback of audio, waits for all samples to be released (https://switchbrew.org/wiki/Audio_services#StartAudioOut)
        */
        void StopAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Appends sample data to the output buffer (https://switchbrew.org/wiki/Audio_services#AppendAudioOutBuffer)
        */
        void AppendAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to the sample release KEvent (https://switchbrew.org/wiki/Audio_services#AppendAudioOutBuffer)
         */
        void RegisterBufferEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the IDs of all pending released buffers (https://switchbrew.org/wiki/Audio_services#GetReleasedAudioOutBuffer)
         */
        void GetReleasedAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Checks if the given buffer ID is in the playback queue (https://switchbrew.org/wiki/Audio_services#ContainsAudioOutBuffer)
         */
        void ContainsAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
