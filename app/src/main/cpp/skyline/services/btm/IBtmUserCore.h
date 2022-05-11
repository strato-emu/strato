// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::btm {
    /**
     * @brief IBcatService is used to interact BLE (Bluetooth Low Energy) devices
     * @url https://switchbrew.org/wiki/BTM_services#IBtmUserCore
     */
    class IBtmUserCore : public BaseService {
      private:
        std::shared_ptr<type::KEvent> bleScanEvent;
        std::shared_ptr<type::KEvent> bleConnectionEvent;
        std::shared_ptr<type::KEvent> bleServiceDiscoveryEvent;
        std::shared_ptr<type::KEvent> bleMtuConfigEvent;

      public:
        IBtmUserCore(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/BTM_services#AcquireBleScanEvent_2
         */
        Result AcquireBleScanEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/BTM_services#AcquireBleConnectionEvent_2
         */
        Result AcquireBleConnectionEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/BTM_services#AcquireBleServiceDiscoveryEvent_2
         */
        Result AcquireBleServiceDiscoveryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/BTM_services#AcquireBleMtuConfigEvent_2
         */
        Result AcquireBleMtuConfigEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IBtmUserCore, AcquireBleScanEvent),
            SFUNC(0x11, IBtmUserCore, AcquireBleConnectionEvent),
            SFUNC(0x1A, IBtmUserCore, AcquireBleServiceDiscoveryEvent),
            SFUNC(0x21, IBtmUserCore, AcquireBleMtuConfigEvent)
        )
    };
}
