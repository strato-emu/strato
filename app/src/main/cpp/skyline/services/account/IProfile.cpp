// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IProfile.h"

namespace skyline::service::account {
    IProfile::IProfile(const DeviceState &state, ServiceManager &manager, const UserId &userId) : userId(userId), BaseService(state, manager, Service::account_IProfile, "account:IProfile", {
        {0x0, SFUNC(IProfile::Get)}
    }) {}

    void IProfile::Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct AccountUserData {
            u32 _unk0_;                        //!< Unknown.
            u32 iconID;                        //!< Icon ID (0 = Mii, the rest are character icon IDs).
            u8 iconBackgroundColorID;          //!< Profile icon background color ID
            std::array<u8, 0x7> _unk1_;        //!< Unknown.
            std::array<u8, 0x10> miiID;        //!< Some ID related to the Mii? All zeros when a character icon is used.
            std::array<u8, 0x60> _unk2_;       //!< Unknown.
        };

        struct {
            UserId uid;                        //!< The UID of the corresponding account
            u64 lastEditTimestamp;             //!< A POSIX UTC timestamp denoting the last account edit.
            std::array<char, 0x20> nickname;   //!< UTF-8 Nickname.
        } accountProfileBase = {.uid = userId};

        std::string username = state.settings->GetString("username_value");
        size_t usernameSize = std::min(accountProfileBase.nickname.size(), username.size());
        std::memcpy(accountProfileBase.nickname.data(), username.c_str(), usernameSize);

        AccountUserData *userData = state.process->GetPointer<AccountUserData>(request.outputBuf.at(0).address);
        userData->iconBackgroundColorID = 0x1; // Color indexing starts at 0x1

        response.Push(accountProfileBase);
    }
}
