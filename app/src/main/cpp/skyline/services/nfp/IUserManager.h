// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::nfp {
    /**
     * @brief IUserManager or nfp:user is used by applications to open an IUser instance for accessing NFC devices (https://switchbrew.org/wiki/NFC_services#nfp:user)
     */
    class IUserManager : public BaseService {
      public:
        IUserManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This opens an IUser that can be used by applications to access NFC devices
         */
        Result CreateUserInterface(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
