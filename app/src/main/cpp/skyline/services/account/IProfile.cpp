// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IProfile.h"

namespace skyline::service::account {
    IProfile::IProfile(const DeviceState &state, ServiceManager &manager, const UserId &userId) : BaseService(state, manager, Service::account_IProfile, "account:IProfile", {
        {0x0, SFUNC(IProfile::Get)}
    }) {}

    void IProfile::Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        AccountUserData userData{};
        AccountProfileBase profileBase{};

        std::string username = state.settings->GetString("username_value");
        size_t usernameSize = std::min(sizeof(profileBase), username.size());
        std::memcpy(profileBase.nickname, username.c_str(), usernameSize);
        profileBase.nickname[usernameSize] = '\0';

        state.process->WriteMemory(userData, request.outputBuf.at(0).address);
        response.Push(profileBase);
    }
}
