// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::lbl {
    /**
     * @url https://switchbrew.org/wiki/Backlight_services#lbl
     */
    class ILblController : public BaseService {
      private:
        bool vrModeEnabled{false};
        float currentBrightnessSettingForVrMode{1.0f};

      public:
        ILblController(const DeviceState &state, ServiceManager &manager);

        Result SetBrightnessReflectionDelayLevel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetBrightnessReflectionDelayLevel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetCurrentAmbientLightSensorMapping(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetCurrentAmbientLightSensorMapping(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetCurrentBrightnessSettingForVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetCurrentBrightnessSettingForVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Backlight_services#EnableVrMode
         */
        Result EnableVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Backlight_services#DisableVrMode
         */
        Result DisableVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Backlight_services#IsVrModeEnabled
         */
        Result IsVrModeEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x11, ILblController, SetBrightnessReflectionDelayLevel),
            SFUNC(0x12, ILblController, GetBrightnessReflectionDelayLevel),
            SFUNC(0x15, ILblController, SetCurrentAmbientLightSensorMapping),
            SFUNC(0x16, ILblController, GetCurrentAmbientLightSensorMapping),
            SFUNC(0x18, ILblController, SetCurrentBrightnessSettingForVrMode),
            SFUNC(0x19, ILblController, GetCurrentBrightnessSettingForVrMode),
            SFUNC(0x1A, ILblController, EnableVrMode),
            SFUNC(0x1B, ILblController, DisableVrMode),
            SFUNC(0x1C, ILblController, IsVrModeEnabled)
        )
    };
}
