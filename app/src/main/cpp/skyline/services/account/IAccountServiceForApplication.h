// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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

            constexpr bool operator==(const UserId &userId) const = default;

            constexpr bool operator!=(const UserId &userId) const = default;
        };

        /**
        * @brief IAccountServiceForApplication or acc:u0 provides functions for reading user information
        * @url https://switchbrew.org/wiki/Account_services#acc:u0
        */
        class IAccountServiceForApplication : public BaseService {
          private:
            std::vector<account::UserId> openedUsers;

            /**
             * @brief Writes a vector of 128-bit user IDs to an output buffer
             */
            Result WriteUserList(span <u8> buffer, std::vector<UserId> userIds);

          public:
            IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager);

            Result GetUserCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result GetUserExistence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result ListAllUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result ListOpenUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result GetLastOpenedUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result GetProfile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @url https://switchbrew.org/wiki/Account_services#IsUserRegistrationRequestPermitted
             */
            Result IsUserRegistrationRequestPermitted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @url https://switchbrew.org/wiki/Account_services#TrySelectUserWithoutInteraction
             */
            Result TrySelectUserWithoutInteraction(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @url https://switchbrew.org/wiki/Account_services#InitializeApplicationInfoV0
             */
            Result InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result GetBaasAccountManagerForApplication(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result StoreSaveDataThumbnail(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result LoadOpenContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result ListOpenContextStoredUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
            * @url https://switchbrew.org/wiki/Account_services#InitializeApplicationInfo
            */
            Result InitializeApplicationInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result ListQualifiedUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result IsUserAccountSwitchLocked(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            Result InitializeApplicationInfoV2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

          SERVICE_DECL(
              SFUNC(0x0, IAccountServiceForApplication, GetUserCount),
              SFUNC(0x1, IAccountServiceForApplication, GetUserExistence),
              SFUNC(0x2, IAccountServiceForApplication, ListAllUsers),
              SFUNC(0x3, IAccountServiceForApplication, ListOpenUsers),
              SFUNC(0x4, IAccountServiceForApplication, GetLastOpenedUser),
              SFUNC(0x5, IAccountServiceForApplication, GetProfile),
              SFUNC(0x32, IAccountServiceForApplication, IsUserRegistrationRequestPermitted),
              SFUNC(0x33, IAccountServiceForApplication, TrySelectUserWithoutInteraction),
              SFUNC(0x64, IAccountServiceForApplication, InitializeApplicationInfoV0),
              SFUNC(0x65, IAccountServiceForApplication, GetBaasAccountManagerForApplication),
              SFUNC(0x6E, IAccountServiceForApplication, StoreSaveDataThumbnail),
              SFUNC(0x82, IAccountServiceForApplication, LoadOpenContext),
              SFUNC(0x83, IAccountServiceForApplication, ListOpenContextStoredUsers),
              SFUNC(0x8C, IAccountServiceForApplication, InitializeApplicationInfo),
              SFUNC(0x8D, IAccountServiceForApplication, ListQualifiedUsers),
              SFUNC(0x96, IAccountServiceForApplication, IsUserAccountSwitchLocked),
              SFUNC(0xA0, IAccountServiceForApplication, InitializeApplicationInfoV2)
          )
        };
    }

    namespace constant {
        constexpr service::account::UserId DefaultUserId{0x0000000000000001, 0x0000000000000000}; //!< The default user ID
    }
}
