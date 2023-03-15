// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#pragma once

#include <audio_core/core/hle/kernel/k_event.h>
#include <audio_core/core/hle/kernel/k_transfer_memory.h>
#include <audio_core/renderer/audio_renderer.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
    * @brief IAudioRenderer is used to control an audio renderer output
    * @url https://switchbrew.org/wiki/Audio_services#IAudioRenderer
    */
    class IAudioRenderer : public BaseService {
      private:
        std::shared_ptr<type::KEvent> renderedEvent;
        KernelShim::KEvent renderedEventWrapper;
        KernelShim::KTransferMemory transferMemoryWrapper;
        AudioCore::AudioRenderer::Renderer impl;

      public:
        IAudioRenderer(const DeviceState &state, ServiceManager &manager,
                       AudioCore::AudioRenderer::Manager &rendererManager,
                       const AudioCore::AudioRendererParameterInternal &params,
                       u64 transferMemorySize, u32 processHandle, u64 appletResourceUserId, i32 sessionId);

        ~IAudioRenderer();

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetSampleRate
         */
        Result GetSampleRate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetSampleCount
        */
        Result GetSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @url https://switchbrew.org/wiki/Audio_services#GetMixBufferCount
        */
        Result GetMixBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @url https://switchbrew.org/wiki/Audio_services#GetAudioRendererState (stubbed)?
        */
        Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result RequestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result Stop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result QuerySystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetRenderingTimeLimit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetRenderingTimeLimit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetVoiceDropParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetVoiceDropParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
            SFUNC(0x0, IAudioRenderer, GetSampleRate),
            SFUNC(0x1, IAudioRenderer, GetSampleCount),
            SFUNC(0x2, IAudioRenderer, GetMixBufferCount),
            SFUNC(0x3, IAudioRenderer, GetState),
            SFUNC(0x4, IAudioRenderer, RequestUpdate),
            SFUNC(0x5, IAudioRenderer, Start),
            SFUNC(0x6, IAudioRenderer, Stop),
            SFUNC(0x7, IAudioRenderer, QuerySystemEvent),
            SFUNC(0x8, IAudioRenderer, SetRenderingTimeLimit),
            SFUNC(0x9, IAudioRenderer, GetRenderingTimeLimit),
            SFUNC(0xA, IAudioRenderer, RequestUpdate),
            SFUNC(0xC, IAudioRenderer, SetVoiceDropParameter),
            SFUNC(0xD, IAudioRenderer, GetVoiceDropParameter)
      )
    };
}
