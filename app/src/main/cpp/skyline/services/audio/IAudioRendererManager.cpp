// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#include <audio_core/common/audio_renderer_parameter.h>
#include <audio_core/audio_render_manager.h>
#include <common/utils.h>
#include <audio.h>
#include "IAudioRenderer.h"
#include "IAudioDevice.h"
#include "IAudioRendererManager.h"

namespace skyline::service::audio {
    IAudioRendererManager::IAudioRendererManager(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager) {}

    Result IAudioRendererManager::OpenAudioRenderer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto &params{request.Pop<AudioCore::AudioRendererParameterInternal>()};
        u64 transferMemorySize{request.Pop<u64>()};
        u64 appletResourceUserId{request.Pop<u64>()};
        auto transferMemoryHandle{request.copyHandles.at(0)};
        auto processHandle{request.copyHandles.at(1)};

        i32 sessionId{state.audio->audioRendererManager->GetSessionId()};
        if (sessionId == -1) {
            Logger::Warn("Out of audio renderer sessions!");
            return Result{Service::Audio::ResultOutOfSessions};
        }

        manager.RegisterService(std::make_shared<IAudioRenderer>(state, manager,
                                                                 *state.audio->audioRendererManager,
                                                                 params,
                                                                 transferMemorySize, processHandle, appletResourceUserId, sessionId),
                                session, response);

        return {};
    }

    Result IAudioRendererManager::GetWorkBufferSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto &params{request.Pop<AudioCore::AudioRendererParameterInternal>()};

        u64 size{};
        auto err{state.audio->audioRendererManager->GetWorkBufferSize(params, size)};
        if (err.IsError())
            Logger::Warn("Failed to calculate work buffer size");

        response.Push<u64>(size);

        return Result{err};
    }

    Result IAudioRendererManager::GetAudioDeviceService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 appletResourceUserId{request.Pop<u64>()};
        manager.RegisterService(std::make_shared<IAudioDevice>(state, manager, appletResourceUserId, util::MakeMagic<u32>("REV1")), session, response);
        return {};
    }

    Result IAudioRendererManager::GetAudioDeviceServiceWithRevisionInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 revision{request.Pop<u32>()};
        u64 appletResourceUserId{request.Pop<u64>()};
        manager.RegisterService(std::make_shared<IAudioDevice>(state, manager, appletResourceUserId, revision), session, response);
        return {};
    }

}
