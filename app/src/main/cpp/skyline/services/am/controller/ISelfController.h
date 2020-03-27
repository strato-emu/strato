// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This has functions relating to an application's own current status (https://switchbrew.org/wiki/Applet_Manager_services#ISelfController)
     */
    class ISelfController : public BaseService {
      public:
        ISelfController(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This function takes a u8 bool flag and no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetOperationModeChangedNotification)
         */
        void SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function takes a u8 bool flag and no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetPerformanceModeChangedNotification)
         */
        void SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function takes 3 unknown u8 values and has no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#GetCurrentFocusState)
         */
        void SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function takes a u8 bool flag and has no output (Stubbed) (https://switchbrew.org/wiki/Applet_Manager_services#SetOutOfFocusSuspendingEnabled)
         */
        void SetOutOfFocusSuspendingEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function returns an output u64 LayerId (https://switchbrew.org/wiki/Applet_Manager_services#CreateManagedDisplayLayer)
         */
        void CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
