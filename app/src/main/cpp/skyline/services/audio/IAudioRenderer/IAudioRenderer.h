// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include <audio.h>
#include "memory_pool.h"
#include "effect.h"
#include "voice.h"
#include "revision_info.h"

namespace skyline {
    namespace constant {
        constexpr u8 BufferAlignment{0x40}; //!< The alignment for all audren buffers
    }

    namespace service::audio::IAudioRenderer {
        /**
         * @brief The parameters used to configure an IAudioRenderer
         */
        struct AudioRendererParameters {
            u32 sampleRate;
            u32 sampleCount;
            u32 mixBufferCount;
            u32 subMixCount;
            u32 voiceCount;
            u32 sinkCount;
            u32 effectCount;
            u32 performanceManagerCount;
            u32 voiceDropEnable;
            u32 splitterCount;
            u32 splitterDestinationDataCount;
            u32 _unk0_;
            u32 revision;
        };
        static_assert(sizeof(AudioRendererParameters) == 0x34);

        /**
         * @brief Header containing information about the software side audren implementation and the sizes of all input data
         */
        struct UpdateDataHeader {
            u32 revision;
            u32 behaviorSize;
            u32 memoryPoolSize;
            u32 voiceSize;
            u32 voiceResourceSize;
            u32 effectSize;
            u32 mixSize;
            u32 sinkSize;
            u32 performanceManagerSize;
            u32 _unk0_;
            u32 elapsedFrameCountInfoSize;
            u32 _unk1_[4];
            u32 totalSize;
        };
        static_assert(sizeof(UpdateDataHeader) == 0x40);

        /**
        * @brief IAudioRenderer is used to control an audio renderer output
        * @url https://switchbrew.org/wiki/Audio_services#IAudioRenderer
        */
        class IAudioRenderer : public BaseService {
          private:
            AudioRendererParameters parameters;
            RevisionInfo revisionInfo{}; //!< Stores info about supported features for the audren revision used
            std::shared_ptr<skyline::audio::AudioTrack> track; //!< The audio track associated with the audio renderer
            std::shared_ptr<type::KEvent> systemEvent; //!< The KEvent that is signalled when the DSP has processed all the commands
            std::vector<MemoryPool> memoryPools;
            std::vector<Effect> effects;
            std::vector<Voice> voices;
            std::array<i16, constant::MixBufferSize * constant::StereoChannelCount> sampleBuffer{}; //!< The final output data that is appended to the stream
            skyline::audio::AudioOutState playbackState{skyline::audio::AudioOutState::Stopped};

            /**
             * @brief Obtains new sample data from voices and mixes it together into the sample buffer
             * @return The amount of samples present in the buffer
             */
            void MixFinalBuffer();

            /**
             * @brief Appends all released buffers with new mixed sample data
             */
            void UpdateAudio();

          public:
            /**
             * @param parameters The parameters to use for rendering
             */
            IAudioRenderer(const DeviceState &state, ServiceManager &manager, AudioRendererParameters &parameters);

            /**
             * @brief Closes the audio track
             */
            ~IAudioRenderer();

            /**
             * @brief Returns the sample rate
             * @url https://switchbrew.org/wiki/Audio_services#GetSampleRate
             */
            Result GetSampleRate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief Returns the sample count
             * @url https://switchbrew.org/wiki/Audio_services#GetSampleCount
            */
            Result GetSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief Returns the number of mix buffers
            * @url https://switchbrew.org/wiki/Audio_services#GetMixBufferCount
            */
            Result GetMixBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief Returns the state of the renderer
            * @url https://switchbrew.org/wiki/Audio_services#GetAudioRendererState (stubbed)?
            */
            Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief Updates the audio renderer state and appends new data to playback buffers
             */
            Result RequestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief Start the audio stream from the renderer
             */
            Result Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief Stop the audio stream from the renderer
             */
            Result Stop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief Returns a handle to the sample release KEvent
             */
            Result QuerySystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            SERVICE_DECL(
                SFUNC(0x0, IAudioRenderer, GetSampleRate),
                SFUNC(0x1, IAudioRenderer, GetSampleCount),
                SFUNC(0x2, IAudioRenderer, GetMixBufferCount),
                SFUNC(0x3, IAudioRenderer, GetState),
                SFUNC(0x4, IAudioRenderer, RequestUpdate),
                SFUNC(0x5, IAudioRenderer, Start),
                SFUNC(0x6, IAudioRenderer, Stop),
                SFUNC(0x7, IAudioRenderer, QuerySystemEvent),
                SFUNC(0xA, IAudioRenderer, RequestUpdate)
            )
        };
    }
}
