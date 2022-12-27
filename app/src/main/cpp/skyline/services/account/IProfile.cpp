// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <vfs/os_backing.h>
#include <fcntl.h>
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

        size_t usernameSize{std::min(accountProfileBase.nickname.size() - 1, (*state.settings->usernameValue).size())};
        std::memcpy(accountProfileBase.nickname.data(), (*state.settings->usernameValue).c_str(), usernameSize);

        response.Push(accountProfileBase);

        return {};
    }

    Result IProfile::GetImageSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::shared_ptr<vfs::Backing> profileImageIcon{GetProfilePicture()};
        response.Push(static_cast<u32>(profileImageIcon->size));

        return {};
    }

    Result IProfile::LoadImage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::shared_ptr<vfs::Backing> profileImageIcon{GetProfilePicture()};

        profileImageIcon->Read(span(request.outputBuf.at(0)).first(profileImageIcon->size), 0);
        response.Push(static_cast<u32>(profileImageIcon->size));

        return {};
    }

    std::shared_ptr<vfs::Backing> IProfile::GetProfilePicture() {
        std::string profilePicturePath{*state.settings->profilePictureValue};
        int fd{open((profilePicturePath).c_str(), O_RDONLY)};
        if (fd < 0)
            // If we can't find the profile picture then just return the placeholder profile picture
            return state.os->assetFileSystem->OpenFile("profile_picture.jpeg");
        else
            return std::make_shared<vfs::OsBacking>(fd, true);
    }
}
