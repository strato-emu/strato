// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#include <audio_core/audio_out_manager.h>
#include <audio.h>
#include <kernel/types/KProcess.h>
#include "IAudioOutManager.h"
#include "IAudioOut.h"

namespace skyline::service::audio {
    IAudioOutManager::IAudioOutManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAudioOutManager::ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::memset(request.outputBuf.at(0).data(), 0, request.outputBuf.at(0).size());
        request.outputBuf.at(0).copy_from(constant::DefaultAudioOutName);
        response.Push<u32>(1); // One audio out
        return {};
    }

    Result IAudioOutManager::OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto &inputParams{request.Pop<AudioCore::AudioOut::AudioOutParameter>()};
        auto appletResourceUserId{request.Pop<u64>()};
        std::string_view deviceName{request.inputBuf.at(0).as_string(true)};
        auto handle{request.copyHandles.at(0)};

        auto &audioOutManager{*state.audio->audioOutManager};
        if (auto result{audioOutManager.LinkToManager()}; result.IsError()) {
            Logger::Warn("Failed to link Audio Out to manager");
            return Result{result};
        }

        size_t sessionId{};
        if (auto result{audioOutManager.AcquireSessionId(sessionId)}; result.IsError()) {
            Logger::Warn("Failed to acquire audio session");
            return Result{result};
        }

        auto audioOut{std::make_shared<IAudioOut>(state, manager, sessionId, deviceName, inputParams, handle, appletResourceUserId)};
        manager.RegisterService(audioOut, session, response);

        audioOutManager.sessions[sessionId] = audioOut->impl;
        audioOutManager.applet_resource_user_ids[sessionId] = appletResourceUserId;

        auto &outSystem{audioOut->impl->GetSystem()};
        response.Push<AudioCore::AudioOut::AudioOutParameterInternal>({
            .sample_rate = outSystem.GetSampleRate(),
            .channel_count = outSystem.GetChannelCount(),
            .sample_format = static_cast<u32>(outSystem.GetSampleFormat()),
            .state = static_cast<u32>(outSystem.GetState())
        });

        std::memset(request.outputBuf.at(0).data(), 0, request.outputBuf.at(0).size());

        request.outputBuf.at(0).copy_from(outSystem.GetName());

        return {};
    }
}
