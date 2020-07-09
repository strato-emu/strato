// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioDevice is used by applications to query audio device info (https://switchbrew.org/wiki/Audio_services#IAudioDevice)
     */
    class IAudioDevice : public BaseService {
      private:
        std::shared_ptr<type::KEvent> systemEvent; //!< The KEvent that is signalled on audio device changes

      public:
        IAudioDevice(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns a list of the available audio devices (https://switchbrew.org/wiki/Audio_services#ListAudioDeviceName)
         */
        void ListAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the volume of an audio output (https://switchbrew.org/wiki/Audio_services#SetAudioDeviceOutputVolume)
         */
        void SetAudioDeviceOutputVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the active audio output device
         */
        void GetActiveAudioDeviceName(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the audio device system event
         */
        void QueryAudioDeviceSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns the current output devices channel count
         */
        void GetActiveChannelCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
