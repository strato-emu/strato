// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IStorageAccessor.h"
#include "IStorage.h"

namespace skyline::service::am {
    IStorage::IStorage(const DeviceState &state, ServiceManager &manager, size_t size) : content(size), BaseService(state, manager, {
        {0x0, SFUNC(IStorage::Open)}
    }) {}

    Result IStorage::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IStorageAccessor>(state, manager, shared_from_this()), session, response);
        return {};
    }
}
