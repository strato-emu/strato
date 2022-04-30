// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <audio/common.h>
#include "IAudioDevice.h"

namespace skyline::service::audio {
    IAudioDevice::IAudioDevice(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager),
          systemEvent(std::make_shared<type::KEvent>(state, true)),
          outputEvent(std::make_shared<type::KEvent>(state, true)),
          inputEvent(std::make_shared<type::KEvent>(state, true)) {}

    Result IAudioDevice::ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        span buffer{request.outputBuf.at(0)};
        std::array<std::string_view, 3> devices{"AudioTvOutput\0", "AudioStereoJackOutput\0", "AudioBuiltInSpeakerOutput\0"};
        for (std::string_view deviceName : devices) {
            if (deviceName.size() > buffer.size())
                throw exception("The buffer supplied to ListAudioDeviceName is too small");
            buffer.copy_from(deviceName);
            buffer = buffer.subspan(deviceName.size());
        }
        response.Push<u32>(devices.size());
        return {};
    }

    Result IAudioDevice::SetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IAudioDevice::GetActiveAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string_view deviceName{"AudioStereoJackOutput\0"};
        if (deviceName.size() > request.outputBuf.at(0).size())
            throw exception("The buffer supplied to GetActiveAudioDeviceName is too small");
        request.outputBuf.at(0).copy_from(deviceName);
        return {};
    }

    Result IAudioDevice::QueryAudioDeviceSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(systemEvent)};
        Logger::Debug("Audio Device System Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioDevice::GetActiveChannelCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(constant::StereoChannelCount);
        return {};
    }

    Result IAudioDevice::QueryAudioDeviceInputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(inputEvent)};
        Logger::Debug("Audio Device Input Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioDevice::QueryAudioDeviceOutputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(outputEvent)};
        Logger::Debug("Audio Device Output Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }
}
