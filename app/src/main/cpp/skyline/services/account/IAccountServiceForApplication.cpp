// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IManagerForApplication.h"
#include "IProfile.h"
#include "IAccountServiceForApplication.h"

namespace skyline::service::account {
    IAccountServiceForApplication::IAccountServiceForApplication(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x1, SFUNC(IAccountServiceForApplication::GetUserExistence)},
        {0x2, SFUNC(IAccountServiceForApplication::ListAllUsers)},
        {0x3, SFUNC(IAccountServiceForApplication::ListOpenUsers)},
        {0x4, SFUNC(IAccountServiceForApplication::GetLastOpenedUser)},
        {0x5, SFUNC(IAccountServiceForApplication::GetProfile)},
        {0x64, SFUNC(IAccountServiceForApplication::InitializeApplicationInfoV0)},
        {0x65, SFUNC(IAccountServiceForApplication::GetBaasAccountManagerForApplication)}
    }) {}

    Result IAccountServiceForApplication::GetUserExistence(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<UserId>();

        // ID can't be zero
        if (id == UserId{})
            return result::NullArgument;

        response.Push<u32>(id == constant::DefaultUserId);
        return {};
    }

    Result IAccountServiceForApplication::ListAllUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        try {
            // We only support one active user currently so hardcode this and ListOpenUsers
            return WriteUserList(request.outputBuf.at(0), {constant::DefaultUserId});
        } catch (const std::out_of_range &e) {
            return result::InvalidInputBuffer;
        }
    }

    Result IAccountServiceForApplication::ListOpenUsers(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        try {
            return WriteUserList(request.outputBuf.at(0), {constant::DefaultUserId});
        } catch (const std::out_of_range &e) {
            return result::InvalidInputBuffer;
        }
    }

    Result IAccountServiceForApplication::WriteUserList(ipc::OutputBuffer buffer, std::vector<UserId> userIds) {
        std::span outputUserIds(state.process->GetPointer<UserId>(buffer.address), buffer.size / sizeof(UserId));

        for (auto &userId : outputUserIds) {
            if (userIds.empty()) {
                userId = UserId{};
            } else {
                userId = userIds.back();
                userIds.pop_back();
            }
        }

        return {};
    }

    Result IAccountServiceForApplication::GetLastOpenedUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(constant::DefaultUserId);
        return {};
    }

    Result IAccountServiceForApplication::InitializeApplicationInfoV0(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IAccountServiceForApplication::GetProfile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<UserId>();
        if (id != constant::DefaultUserId)
            return result::UserNotFound;

        manager.RegisterService(std::make_shared<IProfile>(state, manager, id), session, response);
        return {};
    }

    Result IAccountServiceForApplication::GetBaasAccountManagerForApplication(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<UserId>();
        if (id == UserId{})
            return result::NullArgument;

        manager.RegisterService(SRVREG(IManagerForApplication), session, response);
        return {};
    }
}
