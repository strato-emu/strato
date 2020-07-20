// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline {
    namespace service::account {
        /**
         * @brief This hold an account's user ID
         */
        struct UserId {
            u64 upper; //!< The upper 64 bits of the user ID
            u64 lower; //!< The lower 64 bits of the user ID

            inline constexpr bool operator==(const UserId &userId) {
                return upper == userId.upper && lower == userId.lower;
            }

            inline constexpr bool operator!=(const UserId &userId) {
                return !(*this == userId);
            }
        };
        /**
        * @brief IAccountServiceForApplication or acc:u0 provides functions for reading user information (https://switchbrew.org/wiki/Account_services#acc:u0)
        */
        class IAccountServiceForApplication : public BaseService {
          public:
            IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager);

            /**
            * @brief This checks if the given user ID exists
            */
            void GetUserExistence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This returns the user ID of the last active user on the console
            */
            void GetLastOpenedUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This provides information about the running application for account services to use (https://switchbrew.org/wiki/Account_services#InitializeApplicationInfoV0)
            */
            void InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief This returns a handle to an IProfile which can be used for reading user information
             */
            void GetProfile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This returns a handle to an IManagerForApplication which can be used for reading Nintendo Online info
            */
            void GetBaasAccountManagerForApplication(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
        };
    }

    namespace constant {
        constexpr service::account::UserId DefaultUserId = {0x0000000000000001, 0x0000000000000000}; //!< The default user ID
    }
}