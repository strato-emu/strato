// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bt {
    /**
     * @brief IBluetoothUser is used to interact with BLE (Bluetooth Low Energy) devices
     * @url https://switchbrew.org/wiki/BTM_services#btm:u
     */
    class IBluetoothUser : public BaseService {
      public:
        IBluetoothUser(const DeviceState &state, ServiceManager &manager);
    };
}