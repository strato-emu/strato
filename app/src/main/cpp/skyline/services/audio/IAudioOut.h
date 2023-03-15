// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#pragma once

#include <audio_core/out/audio_out_system.h>
#include <audio_core/out/audio_out.h>
#include <audio_core/core/hle/kernel/k_event.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioOut is a service opened when OpenAudioOut is called by IAudioOutManager
     * @url https://switchbrew.org/wiki/Audio_services#IAudioOut
     */
    class IAudioOut : public BaseService {
      private:
        std::shared_ptr<type::KEvent> releaseEvent;
        KernelShim::KEvent releaseEventWrapper;

        u32 sampleRate;
        u8 channelCount;

      public:
        std::shared_ptr<AudioCore::AudioOut::Out> impl;

        IAudioOut(const DeviceState &state, ServiceManager &manager, size_t sessionId,
                  std::string_view deviceName, AudioCore::AudioOut::AudioOutParameter parameters,
                  KHandle handle, u32 appletResourceUserId);

        ~IAudioOut();

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetAudioOutState
         */
        Result GetAudioOutState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @url https://switchbrew.org/wiki/Audio_services#StartAudioOut
        */
        Result StartAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @url https://switchbrew.org/wiki/Audio_services#StartAudioOut
        */
        Result StopAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @url https://switchbrew.org/wiki/Audio_services#AppendAudioOutBuffer
        */
        Result AppendAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#AppendAudioOutBuffer
         */
        Result RegisterBufferEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetReleasedAudioOutBuffer
         */
        Result GetReleasedAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#ContainsAudioOutBuffer
         */
        Result ContainsAudioOutBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetAudioOutBufferCount
         */
        Result GetAudioOutBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetAudioOutPlayedSampleCount
         */
        Result GetAudioOutPlayedSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#FlushAudioOutBuffers
         */
        Result FlushAudioOutBuffers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#SetAudioOutVolume
         */
        Result SetAudioOutVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetAudioOutVolume
         */
        Result GetAudioOutVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
            SFUNC(0x0, IAudioOut, GetAudioOutState),
            SFUNC(0x1, IAudioOut, StartAudioOut),
            SFUNC(0x2, IAudioOut, StopAudioOut),
            SFUNC(0x3, IAudioOut, AppendAudioOutBuffer),
            SFUNC(0x4, IAudioOut, RegisterBufferEvent),
            SFUNC(0x5, IAudioOut, GetReleasedAudioOutBuffer),
            SFUNC(0x6, IAudioOut, ContainsAudioOutBuffer),
            SFUNC(0x7, IAudioOut, AppendAudioOutBuffer),
            SFUNC(0x8, IAudioOut, GetReleasedAudioOutBuffer),
            SFUNC(0x9, IAudioOut, GetAudioOutBufferCount),
            SFUNC(0xA, IAudioOut, GetAudioOutPlayedSampleCount),
            SFUNC(0xB, IAudioOut, FlushAudioOutBuffers),
            SFUNC(0xC, IAudioOut, SetAudioOutVolume),
            SFUNC(0xD, IAudioOut, GetAudioOutVolume),
      )
    };
}
