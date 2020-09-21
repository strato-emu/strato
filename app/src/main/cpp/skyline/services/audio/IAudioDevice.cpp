// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <audio/common.h>
#include "IAudioDevice.h"

namespace skyline::service::audio {
    IAudioDevice::IAudioDevice(const DeviceState &state, ServiceManager &manager) : systemEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager) {}

    Result IAudioDevice::ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 offset{};
        for (std::string deviceName : {"AudioTvOutput", "AudioStereoJackOutput", "AudioBuiltInSpeakerOutput"}) {
            if (offset + deviceName.size() + 1 > request.outputBuf.at(0).size)
                throw exception("Too small a buffer supplied to ListAudioDeviceName");

            state.process->WriteMemory(deviceName.c_str(), request.outputBuf.at(0).address + offset, deviceName.size() + 1);
            offset += deviceName.size() + 1;
        }
        return {};
    }

    Result IAudioDevice::SetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IAudioDevice::GetActiveAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string deviceName("AudioStereoJackOutput");

        if (deviceName.size() > request.outputBuf.at(0).size)
            throw exception("Too small a buffer supplied to GetActiveAudioDeviceName");

        state.process->WriteMemory(deviceName.c_str(), request.outputBuf.at(0).address, deviceName.size() + 1);
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
