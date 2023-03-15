// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <audio_core/renderer/audio_device.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioDevice is used by applications to query audio device info
     * @url https://switchbrew.org/wiki/Audio_services#IAudioDevice
     */
    class IAudioDevice : public BaseService {
      private:
        std::shared_ptr<type::KEvent> event; //!< Signalled on all audio device changes
        AudioCore::AudioRenderer::AudioDevice impl;

      public:
        IAudioDevice(const DeviceState &state, ServiceManager &manager, u64 appletResourceUserId, u32 revision);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#ListAudioDeviceName
         */
        Result ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#SetAudioDeviceOutputVolume
         */
        Result SetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Audio_services#GetAudioDeviceOutputVolume
         */
        Result GetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetActiveAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result QueryAudioDeviceSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the current output devices channel count
         */
        Result GetActiveChannelCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result QueryAudioDeviceInputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result QueryAudioDeviceOutputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ListAudioOutputDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IAudioDevice, ListAudioDeviceName),
            SFUNC(0x1, IAudioDevice, SetAudioDeviceOutputVolume),
            SFUNC(0x2, IAudioDevice, GetActiveAudioDeviceName),
            SFUNC(0x3, IAudioDevice, GetActiveAudioDeviceName),
            SFUNC(0x4, IAudioDevice, QueryAudioDeviceSystemEvent),
            SFUNC(0x5, IAudioDevice, GetActiveChannelCount),
            SFUNC(0x6, IAudioDevice, ListAudioDeviceName),
            SFUNC(0x7, IAudioDevice, SetAudioDeviceOutputVolume),
            SFUNC(0x8, IAudioDevice, GetAudioDeviceOutputVolume),
            SFUNC(0xA, IAudioDevice, GetActiveAudioDeviceName),
            SFUNC(0xB, IAudioDevice, QueryAudioDeviceInputEvent),
            SFUNC(0xC, IAudioDevice, QueryAudioDeviceOutputEvent),
            SFUNC(0xD, IAudioDevice, GetActiveAudioDeviceName),
            SFUNC(0xE, IAudioDevice, ListAudioOutputDeviceName)
        )
    };
}
