// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IStorageAccessor.h"
#include "IStorage.h"

namespace skyline::service::am {
    IStorage::IStorage(const DeviceState &state, ServiceManager &manager, bool writable) : writable(writable), BaseService(state, manager) {}

    Result IStorage::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IStorageAccessor>(state, manager, std::static_pointer_cast<IStorage>(shared_from_this())), session, response);
        return {};
    }

    IStorage::~IStorage() = default;
}
