// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IAccountServiceForApplication.h"

namespace skyline::service::account {

    /**
    * @brief IProfile provides functions for reading user profile
    * @url https://switchbrew.org/wiki/Account_services#IProfile
    */
    class IProfile : public BaseService {
      public:
        IProfile(const DeviceState &state, ServiceManager &manager, const UserId &userId);

      private:
        UserId userId;

        /**
         * @brief Returns AccountUserData and AccountProfileBase objects that describe the user's information
         */
        Result Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an AccountProfileBase object that describe the user's information
         */
        Result GetBase(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Account_services#GetImageSize
         */
        Result GetImageSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Account_services#LoadImage
         */
        Result LoadImage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Tries to get the user's profile picture. If not found, returns the default one
         * @return A shared pointer to a Backing object of the profile picture
         */
        std::shared_ptr<vfs::Backing> GetProfilePicture();

        SERVICE_DECL(
            SFUNC(0x0, IProfile, Get),
            SFUNC(0x1, IProfile, GetBase),
            SFUNC(0xA, IProfile, GetImageSize),
            SFUNC(0xB, IProfile, LoadImage)
        )
    };
}
