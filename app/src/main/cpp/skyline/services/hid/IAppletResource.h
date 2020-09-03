// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KProcess.h>
#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::hid {
    /**
     * @brief IAppletResource is used to get a handle to the HID shared memory (https://switchbrew.org/wiki/HID_services#IAppletResource)
     */
    class IAppletResource : public BaseService {
      public:
        IAppletResource(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This opens a handle to HID shared memory (https://switchbrew.org/wiki/HID_services#GetSharedMemoryHandle)
         */
        Result GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
