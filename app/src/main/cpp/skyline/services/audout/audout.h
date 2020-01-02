#pragma once

#include <audio.h>
#include <services/base_service.h>
#include <services/serviceman.h>
#include <kernel/types/KEvent.h>

namespace skyline::service::audout {
    namespace constant {
        constexpr std::string_view DefaultAudioOutName = "DeviceOut"; //!< The default audio output device name
    };

    /**
     * @brief audout:u or IAudioOutManager is used to manage audio outputs (https://switchbrew.org/wiki/Audio_services#audout:u)
     */
    class audoutU : public BaseService {
      public:
        audoutU(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns a list of all available audio outputs (https://switchbrew.org/wiki/Audio_services#ListAudioOuts)
         */
        void ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Creates a new audoutU::IAudioOut object and returns a handle to it (https://switchbrew.org/wiki/Audio_services#OpenAudioOut)
         */
        void OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief IAudioOut is a service opened when OpenAudioOut is called by audout (https://switchbrew.org/wiki/Audio_services#IAudioOut)
     */
    class IAudioOut : public BaseService {
      private:
        std::shared_ptr<audio::AudioTrack> track; //!< The audio track associated with the audio out
        std::shared_ptr<type::KEvent> releaseEvent; //!< The KEvent that is signalled when a buffer has been released
        std::vector<i16> tmpSampleBuffer; //!< A temporary buffer used to store sample data in AppendAudioOutBuffer

      public:
        /**
         * @param channelCount The channel count of the audio data the audio out will be fed
         * @param sampleRate The sample rate of the audio data the audio out will be fed
         */
        IAudioOut(const DeviceState &state, ServiceManager &manager, int channelCount, int sampleRate);

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
