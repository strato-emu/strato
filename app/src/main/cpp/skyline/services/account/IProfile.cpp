// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/settings.h>
#include "IProfile.h"

namespace skyline::service::account {
    // Smallest JPEG file https://github.com/mathiasbynens/small/blob/master/jpeg.jpg
    constexpr std::array<u8, 107> profileImageIcon{
        0xFF, 0xD8, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x03, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x06, 0x04,
        0x04, 0x04, 0x04, 0x04, 0x08, 0x06, 0x06, 0x05, 0x06, 0x09, 0x08, 0x0A,
        0x0A, 0x09, 0x08, 0x09, 0x09, 0x0A, 0x0C, 0x0F, 0x0C, 0x0A, 0x0B, 0x0E,
        0x0B, 0x09, 0x09, 0x0D, 0x11, 0x0D, 0x0E, 0x0F, 0x10, 0x10, 0x11, 0x10,
        0x0A, 0x0C, 0x12, 0x13, 0x12, 0x10, 0x13, 0x0F, 0x10, 0x10, 0x10, 0xFF,
        0xC9, 0x00, 0x0B, 0x08, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x11, 0x00,
        0xFF, 0xCC, 0x00, 0x06, 0x00, 0x10, 0x10, 0x05, 0xFF, 0xDA, 0x00, 0x08,
        0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0xD2, 0xCF, 0x20, 0xFF, 0xD9
    };

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

        size_t usernameSize{std::min(accountProfileBase.nickname.size() - 1, (*state.settings->usernameValue).size())};
        std::memcpy(accountProfileBase.nickname.data(), (*state.settings->usernameValue).c_str(), usernameSize);

        response.Push(accountProfileBase);

        return {};
    }

    Result IProfile::GetImageSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(profileImageIcon.size());
        return {};
    }

    Result IProfile::LoadImage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // TODO: load actual profile image
        request.outputBuf.at(0).copy_from(profileImageIcon);
        response.Push<u32>(profileImageIcon.size());
        return {};
    }
}
