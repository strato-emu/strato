// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/settings.h>
#include "IProfile.h"

namespace skyline::service::account {
    IProfile::IProfile(const DeviceState &state, ServiceManager &manager, const UserId &userId) : userId(userId), BaseService(state, manager) {}

    Result IProfile::Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct AccountUserData {
            u32 _unk0_;
            u32 iconID;                        //!< Icon ID (0 = Mii, the rest are character icon IDs)
            u8 iconBackgroundColorID;          //!< Profile icon background color ID
            u8 _unk1_[0x7];
            std::array<u8, 0x10> miiID;        //!< Some ID related to the Mii? All zeros when a character icon is used
            u8 _unk2_[0x60];
        };

        request.outputBuf.at(0).as<AccountUserData>().iconBackgroundColorID = 0x1; // Color indexing starts at 0x1

        return GetBase(session, request, response);
    }

    Result IProfile::GetBase(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct {
            UserId uid;                        //!< The UID of the corresponding account
            u64 lastEditTimestamp;             //!< A POSIX UTC timestamp denoting the last account edit
            std::array<char, 0x20> nickname;   //!< UTF-8 Nickname
        } accountProfileBase = {
            .uid = userId,
        };

        auto username{state.settings->GetString("username_value")};
        size_t usernameSize{std::min(accountProfileBase.nickname.size() - 1, username.size())};
        std::memcpy(accountProfileBase.nickname.data(), username.c_str(), usernameSize);

        response.Push(accountProfileBase);

        return {};
    }
}
