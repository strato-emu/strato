// SPDX-License-Identifier: MPL-2.0
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

        u32 sampleRate;
        u8 channelCount;

      public:
        /**
         * @param channelCount The channel count of the audio data the audio out will be fed
         * @param sampleRate The sample rate of the audio data the audio out will be fed
         */
        IAudioOut(const DeviceState &state, ServiceManager &manager, u8 channelCount, u32 sampleRate);

        /**
         * @brief Closes the audio track
         */
        ~IAudioOut();

        /**
         * @brief Returns the playback state of the audio output (https://switchbrew.org/wiki/Audio_services#GetAudioOutState)
         */
        Result GetAudioOutState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Starts playback using data from appended samples (https://switchbrew.org/wiki/Audio_services#StartAudioOut)
        */
        Result StartAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Stops playback of audio, waits for all samples to be released (https://switchbrew.org/wiki/Audio_services#StartAudioOut)
        */
        Result StopAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Appends sample data to the output buffer (https://switchbrew.org/wiki/Audio_services#AppendAudioOutBuffer)
        */
        Result AppendAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to the sample release KEvent (https://switchbrew.org/wiki/Audio_services#AppendAudioOutBuffer)
         */
        Result RegisterBufferEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the IDs of all pending released buffers (https://switchbrew.org/wiki/Audio_services#GetReleasedAudioOutBuffer)
         */
        Result GetReleasedAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Checks if the given buffer ID is in the playback queue (https://switchbrew.org/wiki/Audio_services#ContainsAudioOutBuffer)
         */
        Result ContainsAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IAudioOut, GetAudioOutState),
            SFUNC(0x1, IAudioOut, StartAudioOut),
            SFUNC(0x2, IAudioOut, StopAudioOut),
            SFUNC(0x3, IAudioOut, AppendAudioOutBuffer),
            SFUNC(0x4, IAudioOut, RegisterBufferEvent),
            SFUNC(0x5, IAudioOut, GetReleasedAudioOutBuffer),
            SFUNC(0x6, IAudioOut, ContainsAudioOutBuffer),
            SFUNC(0x7, IAudioOut, AppendAudioOutBuffer),
            SFUNC(0x8, IAudioOut, GetReleasedAudioOutBuffer)
        )
    };
}
