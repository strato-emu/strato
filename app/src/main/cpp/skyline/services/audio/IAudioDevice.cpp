// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#include <audio_core/audio_core.h>
#include <audio.h>
#include <kernel/types/KProcess.h>
#include "IAudioDevice.h"

namespace skyline::service::audio {
    IAudioDevice::IAudioDevice(const DeviceState &state, ServiceManager &manager, u64 appletResourceUserId, u32 revision)
        : BaseService{state, manager},
          event{std::make_shared<type::KEvent>(state, true)},
          impl{state.audio->audioSystem, appletResourceUserId, revision} {}

    Result IAudioDevice::ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        span buffer{request.outputBuf.at(0)};

        std::vector<AudioCore::AudioRenderer::AudioDevice::AudioDeviceName> outputNames{};
        u32 writtenCount{impl.ListAudioDeviceName(outputNames, buffer.size() / sizeof(AudioCore::AudioRenderer::AudioDevice::AudioDeviceName))};
        response.Push<u32>(writtenCount);
        buffer.copy_from(outputNames);
        return {};
    }

    Result IAudioDevice::SetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        float volume{request.Pop<float>()};
        auto name{request.inputBuf.at(0).as_string(true)};

        if (name == "AudioTvOutput")
            impl.SetDeviceVolumes(volume);

        return {};
    }

    Result IAudioDevice::GetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto name{request.inputBuf.at(0).as_string(true)};

        response.Push<float>(name == "AudioTvOutput" ? impl.GetDeviceVolume(name) : 1.0f);
        return {};
    }

    Result IAudioDevice::GetActiveAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string_view deviceName{"AudioTvOutput\0"};
        if (deviceName.size() > request.outputBuf.at(0).size())
            throw exception("The buffer supplied to GetActiveAudioDeviceName is too small");
        request.outputBuf.at(0).copy_from(deviceName);
        return {};
    }

    Result IAudioDevice::QueryAudioDeviceSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(event)};
        event->Signal();
        Logger::Debug("Audio Device System Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioDevice::GetActiveChannelCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(state.audio->audioSystem.AudioCore().GetOutputSink().GetDeviceChannels());
        return {};
    }

    Result IAudioDevice::QueryAudioDeviceInputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(event)};
        Logger::Debug("Audio Device Input Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioDevice::QueryAudioDeviceOutputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(event)};
        Logger::Debug("Audio Device Output Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioDevice::ListAudioOutputDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        span buffer{request.outputBuf.at(0)};

        std::vector<AudioCore::AudioRenderer::AudioDevice::AudioDeviceName> outputNames{};
        u32 writtenCount{impl.ListAudioOutputDeviceName(outputNames, buffer.size() / sizeof(AudioCore::AudioRenderer::AudioDevice::AudioDeviceName))};
        response.Push<u32>(writtenCount);
        buffer.copy_from(outputNames);
        return {};
    }
}
