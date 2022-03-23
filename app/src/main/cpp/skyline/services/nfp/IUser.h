// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::nfp {
    /**
     * @brief IUser is used by applications to access NFP (Near Field Proximity) devices
     * @url https://switchbrew.org/wiki/NFC_services#IUser_3
     */
    class IUser : public BaseService {
      public:
        IUser(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Initializes an NFP session
         */
        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @brief Lists available NFP devices
         * @url https://switchbrew.org/wiki/NFC_services#ListDevices
         */
        Result ListDevices(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /*
         * @brief Return the state of the NFP service
         * @url https://switchbrew.org/wiki/NFC_services#GetState_2
         */
        Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IUser, Initialize),
            SFUNC(0x2, IUser, ListDevices),
            SFUNC(0x13, IUser, GetState)
        )
    };
}
