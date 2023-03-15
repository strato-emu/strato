// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2022 yuzu Emulator Project (https://github.com/yuzu-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline {
    namespace constant {
        constexpr std::string_view DefaultAudioOutName{"DeviceOut"}; //!< The default audio output device name
    }

    namespace service::audio {
        /**
         * @brief IAudioOutManager or audout:u is used to manage audio outputs
         * @url https://switchbrew.org/wiki/Audio_services#audout:u
         */
        class IAudioOutManager : public BaseService {
          public:
            IAudioOutManager(const DeviceState &state, ServiceManager &manager);

            /**
             * @brief Returns a list of all available audio outputs
             * @url https://switchbrew.org/wiki/Audio_services#ListAudioOuts
             */
            Result ListAudioOuts(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief Creates a new audoutU::IAudioOut object and returns a handle to it
             * @url https://switchbrew.org/wiki/Audio_services#OpenAudioOut
             */
            Result OpenAudioOut(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            SERVICE_DECL(
                SFUNC(0x0, IAudioOutManager, ListAudioOuts),
                SFUNC(0x1, IAudioOutManager, OpenAudioOut),
                SFUNC(0x2, IAudioOutManager, ListAudioOuts),
                SFUNC(0x3, IAudioOutManager, OpenAudioOut)
            )
        };
    }
}
