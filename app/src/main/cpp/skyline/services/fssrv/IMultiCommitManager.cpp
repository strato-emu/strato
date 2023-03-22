// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IMultiCommitManager.h"

namespace skyline::service::fssrv {
    IMultiCommitManager::IMultiCommitManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IMultiCommitManager::Add(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IMultiCommitManager::Commit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
