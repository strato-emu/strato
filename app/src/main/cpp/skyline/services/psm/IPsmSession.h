// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::psm {

    /**
     * @url https://switchbrew.org/wiki/PTM_services#IPsmSession
     */
    class IPsmSession : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> stateChangeEvent;

      public:
        IPsmSession(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#BindStateChangeEvent
         */
        Result BindStateChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#UnbindStateChangeEvent
         */
        Result UnbindStateChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#SetChargerTypeChangeEventEnabled
         */
        Result SetChargerTypeChangeEventEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#SetPowerSupplyChangeEventEnabled
         */
        Result SetPowerSupplyChangeEventEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#SetBatteryVoltageStateChangeEventEnabled
         */
        Result SetBatteryVoltageStateChangeEventEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
           SFUNC(0x0, IPsmSession, BindStateChangeEvent),
           SFUNC(0x1, IPsmSession, UnbindStateChangeEvent),
           SFUNC(0x2, IPsmSession, SetChargerTypeChangeEventEnabled),
           SFUNC(0x3, IPsmSession, SetPowerSupplyChangeEventEnabled),
           SFUNC(0x4, IPsmSession, SetBatteryVoltageStateChangeEventEnabled)
        )
    };
}
