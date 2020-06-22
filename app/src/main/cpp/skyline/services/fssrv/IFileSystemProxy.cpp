// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <loader/loader.h>
#include "IFileSystemProxy.h"
#include "IStorage.h"

namespace skyline::service::fssrv {
    IFileSystemProxy::IFileSystemProxy(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::fssrv_IFileSystemProxy, "fssrv:IFileSystemProxy", {
        {0x1, SFUNC(IFileSystemProxy::SetCurrentProcess)},
        {0x12, SFUNC(IFileSystemProxy::OpenSdCardFileSystem)},
        {0xc8, SFUNC(IFileSystemProxy::OpenDataStorageByCurrentProcess)}
    }) {}

    void IFileSystemProxy::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = request.Pop<pid_t>();
    }

    void IFileSystemProxy::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(FsType::SdCard, state, manager), session, response);
    }

    void IFileSystemProxy::OpenDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (state.loader->romFs)
            manager.RegisterService(std::make_shared<IStorage>(state.loader->romFs, state, manager), session, response);
        else
            throw exception("Tried to call OpenDataStorageByCurrentProcess without a valid RomFS");
    }
}
