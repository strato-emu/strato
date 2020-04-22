// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IFileSystemProxy.h"

namespace skyline::service::fssrv {
    IFileSystemProxy::IFileSystemProxy(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::fssrv_IFileSystemProxy, "fssrv:IFileSystemProxy", {
        {0x1, SFUNC(IFileSystemProxy::SetCurrentProcess)},
        {0x12, SFUNC(IFileSystemProxy::OpenSdCardFileSystem)}
    }) {}

    void IFileSystemProxy::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = request.Pop<pid_t>();
    }

    void IFileSystemProxy::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(FsType::SdCard, state, manager), session, response);
    }
}
