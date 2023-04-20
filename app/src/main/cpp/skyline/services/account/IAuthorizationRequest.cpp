// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAuthorizationRequest.h"
#include "IAsyncContext.h"

namespace skyline::service::account {
    IAuthorizationRequest::IAuthorizationRequest(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAuthorizationRequest::InvokeWithoutInteractionAsync(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAsyncContext), session, response);
        return {};
    }

    Result IAuthorizationRequest::IsAuthorized(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(0);
        return {};
    }

    Result IAuthorizationRequest::GetAuthorizationCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
