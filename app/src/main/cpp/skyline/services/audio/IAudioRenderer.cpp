// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#include <audio.h>
#include <kernel/types/KProcess.h>
#include "IAudioRenderer.h"

namespace skyline::service::audio {
    IAudioRenderer::IAudioRenderer(const DeviceState &state, ServiceManager &manager,
                                   AudioCore::AudioRenderer::Manager &rendererManager,
                                   const AudioCore::AudioRendererParameterInternal &params,
                                   u64 transferMemorySize, u32 processHandle, u64 appletResourceUserId, i32 sessionId)
        : BaseService{state, manager},
          renderedEvent{std::make_shared<type::KEvent>(state, true)},
          renderedEventWrapper{[renderedEvent = this->renderedEvent]() { renderedEvent->Signal(); },
                              [renderedEvent = this->renderedEvent]() { renderedEvent->ResetSignal(); }},
          transferMemoryWrapper{transferMemorySize},
          impl{state.audio->audioSystem, rendererManager, &renderedEventWrapper} {
        impl.Initialize(params, &transferMemoryWrapper, transferMemorySize, processHandle, appletResourceUserId, sessionId);
    }

    IAudioRenderer::~IAudioRenderer() {
        impl.Finalize();
    }

    Result IAudioRenderer::GetSampleRate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(impl.GetSystem().GetSampleRate());
        return {};
    }

    Result IAudioRenderer::GetSampleCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(impl.GetSystem().GetSampleCount());
        return {};
    }

    Result IAudioRenderer::GetMixBufferCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(impl.GetSystem().GetMixBufferCount());
        return {};
    }

    Result IAudioRenderer::GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(impl.GetSystem().IsActive() ? 0 : 1);
        return {};
    }

    Result IAudioRenderer::RequestUpdate(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto input{request.inputBuf.at(0)};
        auto output{request.outputBuf.at(0)};
        auto performanceOutput{request.outputBuf.size() > 1 ? request.outputBuf.at(1) : span<u8>{}};

        if (auto result{impl.RequestUpdate(input, performanceOutput, output)}; result.IsError()) {
            Logger::Error("Update failed error: 0x{:X}", u32{result});
            return Result{result};
        }

        return {};
    }

    Result IAudioRenderer::Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        impl.GetSystem().Start();
        return {};
    }

    Result IAudioRenderer::Stop(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        impl.GetSystem().Stop();
        return {};
    }

    Result IAudioRenderer::QuerySystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (impl.GetSystem().GetExecutionMode() == AudioCore::ExecutionMode::Manual)
            return Result{Service::Audio::ResultNotSupported};

        auto handle{state.process->InsertItem(renderedEvent)};
        Logger::Debug("System Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioRenderer::SetRenderingTimeLimit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 limit{request.Pop<u32>()};
        impl.GetSystem().SetRenderingTimeLimit(limit);
        return {};
    }

    Result IAudioRenderer::GetRenderingTimeLimit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(impl.GetSystem().GetRenderingTimeLimit());
        return {};
    }

    Result IAudioRenderer::SetVoiceDropParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        float voiceDropParam{request.Pop<float>()};
        impl.GetSystem().SetVoiceDropParameter(voiceDropParam);
        return {};
    }

    Result IAudioRenderer::GetVoiceDropParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<float>(impl.GetSystem().GetVoiceDropParameter());
        return {};
    }
}
