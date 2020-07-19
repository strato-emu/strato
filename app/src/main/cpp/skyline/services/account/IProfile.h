// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IAccountServiceForApplication.h"
#include <services/base_service.h>

namespace skyline::service::account {

    /// UserData
    typedef struct {
        u32 unk_x0;                ///< Unknown.
        u32 iconID;                ///< Icon ID. 0 = Mii, the rest are character icon IDs.
        u8 iconBackgroundColorID;  ///< Profile icon background color ID
        u8 unk_x9[0x7];            ///< Unknown.
        u8 miiID[0x10];            ///< Some ID related to the Mii? All zeros when a character icon is used.
        u8 unk_x20[0x60];          ///< Usually zeros?
    } AccountUserData;

    /// ProfileBase
    typedef struct {
        UserId uid;        ///< \ref AccountUid
        u64 lastEditTimestamp; ///< POSIX UTC timestamp, for the last account edit.
        char nickname[0x20];   ///< UTF-8 Nickname.
    } AccountProfileBase;

    /**
    * @brief IProfile provides functions for reading user profile (https://switchbrew.org/wiki/Account_services#IProfile)
    */
    class IProfile : public BaseService {
      public:
        IProfile(const DeviceState &state, ServiceManager &manager, const UserId &userId);

      private:
        /**
         * @brief This returns AccountUserData (optional) and AccountProfileBase
         */
        void Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
