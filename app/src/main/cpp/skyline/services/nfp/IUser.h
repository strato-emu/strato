// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::nfp {
    /**
     * @brief IUser is used by applications to access NFC devices
     * @url https://switchbrew.org/wiki/NFC_services#IUser
     */
    class IUser : public BaseService {
      public:
        IUser(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Initializes an NFP session
         */
        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IUser, Initialize)
        )
    };
}
