// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <audio/common.h>
#include "IAudioDevice.h"

namespace skyline::service::audio {
    IAudioDevice::IAudioDevice(const DeviceState &state, ServiceManager &manager) : systemEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager) {}

    Result IAudioDevice::ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        span buffer{request.outputBuf.at(0)};
        for (std::string_view deviceName : {"AudioTvOutput\0", "AudioStereoJackOutput\0", "AudioBuiltInSpeakerOutput\0"}) {
            if (deviceName.size() > buffer.size())
                throw exception("The buffer supplied to ListAudioDeviceName is too small");
            buffer.copy_from(deviceName);
            buffer = buffer.subspan(deviceName.size());
        }
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
        state.logger->Debug("Audio Device System Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAudioDevice::GetActiveChannelCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(constant::ChannelCount);
        return {};
    }
}
