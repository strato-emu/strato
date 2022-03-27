// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KSharedMemory.h>
#include <services/serviceman.h>

namespace skyline::service::irs {
    struct SharedIirCore;
    
    namespace result {
        constexpr Result InvalidNpadId(205, 709);
    }
    class IIrSensorServer : public BaseService {
      private:
        SharedIirCore &core;

      public:
        IIrSensorServer(const DeviceState &state, ServiceManager &manager, SharedIirCore &core);

        /**
         * @brief Returns an IirCameraHandle given the NpadIdType
         * @url https://switchbrew.org/wiki/HID_services#GetNpadIrCameraHandle
         */
        Result GetNpadIrCameraHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
        /**
         * @brief Requests a PID and Fuction Level and activates the IR sensor
         * @url https://switchbrew.org/wiki/HID_services#ActivateIrsensorWithFunctionLevel
        */
        Result ActivateIrsensorWithFunctionLevel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the shared memory handle for the IR sensor
         * @url https://switchbrew.org/wiki/HID_services#GetIrsensorSharedMemoryHandle
         */
        Result GetIrsensorSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
          SFUNC(0x130, IIrSensorServer, GetIrsensorSharedMemoryHandle),
          SFUNC(0x137, IIrSensorServer, GetNpadIrCameraHandle),
          SFUNC(0x13F, IIrSensorServer, ActivateIrsensorWithFunctionLevel)
      );
    };
}

