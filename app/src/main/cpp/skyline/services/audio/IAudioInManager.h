// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline {
    namespace service::audio {
        /**
         * @brief IAudioInManager or audin:u is used to manage audio inputs
         * @url https://switchbrew.org/wiki/Audio_services#audin:u
         */
        class IAudioInManager : public BaseService {
          public:
            IAudioInManager(const DeviceState &state, ServiceManager &manager);

            /**
             * @url https://switchbrew.org/wiki/Audio_services#ListAudioIns
             */
            Result ListAudioIns(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            SERVICE_DECL(
                SFUNC(0x0, IAudioInManager, ListAudioIns),
                SFUNC(0x2, IAudioInManager, ListAudioIns),
                SFUNC(0x4, IAudioInManager, ListAudioIns)
            )
        };
    }
}
