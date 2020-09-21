// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::audio {
    /**
     * @brief IAudioRendererManager or audren:u is used to manage audio renderer outputs (https://switchbrew.org/wiki/Audio_services#audren:u)
     */
    class IAudioRendererManager : public BaseService {
      public:
        IAudioRendererManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Creates a new IAudioRenderer object and returns a handle to it
         */
        Result OpenAudioRenderer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Calculates the size of the buffer the guest needs to allocate for IAudioRendererManager
         */
        Result GetAudioRendererWorkBufferSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to an instance of an IAudioDevice (https://switchbrew.org/wiki/Audio_services#GetAudioDeviceService)
         */
        Result GetAudioDeviceService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IAudioRendererManager, OpenAudioRenderer),
            SFUNC(0x1, IAudioRendererManager, GetAudioRendererWorkBufferSize),
            SFUNC(0x2, IAudioRendererManager, GetAudioDeviceService),
            SFUNC(0x4, IAudioRendererManager, GetAudioDeviceService)
        )
    };
}
