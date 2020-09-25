// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline {
    namespace service::account {
        namespace result {
            constexpr Result NullArgument(124, 20);
            constexpr Result InvalidArgument(124, 22);
            constexpr Result InvalidInputBuffer(124, 32);
            constexpr Result UserNotFound(124, 100);
        }

        /**
         * @brief An HOS account's user ID
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
          private:
            /**
             * @brief Writes a vector of 128-bit user IDs to an output buffer
             */
            Result WriteUserList(span<u8> buffer, std::vector<UserId> userIds);

          public:
            IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager);

            /**
            * @brief This checks if the given user ID exists
            */
            Result GetUserExistence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This returns a list of all user accounts on the console
            */
            Result ListAllUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This returns a list of all open user accounts on the console
            */
            Result ListOpenUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This returns the user ID of the last active user on the console
            */
            Result GetLastOpenedUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This provides information about the running application for account services to use (https://switchbrew.org/wiki/Account_services#InitializeApplicationInfoV0)
            */
            Result InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief This returns a handle to an IProfile which can be used for reading user information
             */
            Result GetProfile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @brief This returns a handle to an IManagerForApplication which can be used for reading Nintendo Online info
            */
            Result GetBaasAccountManagerForApplication(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            SERVICE_DECL(
                SFUNC(0x1, IAccountServiceForApplication, GetUserExistence),
                SFUNC(0x2, IAccountServiceForApplication, ListAllUsers),
                SFUNC(0x3, IAccountServiceForApplication, ListOpenUsers),
                SFUNC(0x4, IAccountServiceForApplication, GetLastOpenedUser),
                SFUNC(0x5, IAccountServiceForApplication, GetProfile),
                SFUNC(0x64, IAccountServiceForApplication, InitializeApplicationInfoV0),
                SFUNC(0x65, IAccountServiceForApplication, GetBaasAccountManagerForApplication)
            )
        };
    }

    namespace constant {
        constexpr service::account::UserId DefaultUserId = {0x0000000000000001, 0x0000000000000000}; //!< The default user ID
    }
}
