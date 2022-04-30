// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioDevice is used by applications to query audio device info
     * @url https://switchbrew.org/wiki/Audio_services#IAudioDevice
     */
    class IAudioDevice : public BaseService {
      private:
        std::shared_ptr<type::KEvent> systemEvent; //!< Signalled on all audio device changes
        std::shared_ptr<type::KEvent> inputEvent; //!< Signalled on audio input device changes
        std::shared_ptr<type::KEvent> outputEvent; //!< Signalled on audio output device changes


      public:
        IAudioDevice(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns a list of the available audio devices
         * @url https://switchbrew.org/wiki/Audio_services#ListAudioDeviceName
         */
        Result ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the volume of an audio output
         * @url https://switchbrew.org/wiki/Audio_services#SetAudioDeviceOutputVolume
         */
        Result SetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the active audio output device
         */
        Result GetActiveAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the audio device system event
         */
        Result QueryAudioDeviceSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the current output devices channel count
         */
        Result GetActiveChannelCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result QueryAudioDeviceInputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result QueryAudioDeviceOutputEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IAudioDevice, ListAudioDeviceName),
            SFUNC(0x1, IAudioDevice, SetAudioDeviceOutputVolume),
            SFUNC(0x3, IAudioDevice, GetActiveAudioDeviceName),
            SFUNC(0x4, IAudioDevice, QueryAudioDeviceSystemEvent),
            SFUNC(0x5, IAudioDevice, GetActiveChannelCount),
            SFUNC(0x6, IAudioDevice, ListAudioDeviceName),
            SFUNC(0x7, IAudioDevice, SetAudioDeviceOutputVolume),
            SFUNC(0xA, IAudioDevice, GetActiveAudioDeviceName),
            SFUNC(0xB, IAudioDevice, QueryAudioDeviceInputEvent),
            SFUNC(0xC, IAudioDevice, QueryAudioDeviceOutputEvent)
        )
    };
}
