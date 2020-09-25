// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::hid {
    /**
     * @brief IActiveVibrationDeviceList is used to activate vibration on certain HID devices (https://switchbrew.org/wiki/HID_services#IActiveVibrationDeviceList)
     */
    class IActiveVibrationDeviceList : public BaseService {
      public:
        IActiveVibrationDeviceList(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Activates a vibration device with the specified #VibrationDeviceHandle (https://switchbrew.org/wiki/HID_services#ActivateVibrationDevice)
         */
        Result ActivateVibrationDevice(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IActiveVibrationDeviceList, ActivateVibrationDevice)
        )
    };
}
