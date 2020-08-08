// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <vfs/filesystem.h>
#include "IFile.h"
#include "IFileSystem.h"

namespace skyline::service::fssrv {
    IFileSystem::IFileSystem(std::shared_ptr<vfs::FileSystem> backing, const DeviceState &state, ServiceManager &manager) : backing(backing), BaseService(state, manager, Service::fssrv_IFileSystem, "fssrv:IFileSystem", {
        {0x0, SFUNC(IFileSystem::CreateFile)},
        {0x7, SFUNC(IFileSystem::GetEntryType)},
        {0x8, SFUNC(IFileSystem::OpenFile)},
        {0xa, SFUNC(IFileSystem::Commit)}
    }) {}

    void IFileSystem::CreateFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path = std::string(state.process->GetPointer<char>(request.inputBuf.at(0).address));
        auto mode = request.Pop<u64>();
        auto size = request.Pop<u32>();

        response.errorCode = backing->CreateFile(path, size) ? constant::status::Success : constant::status::PathDoesNotExist;
    }

    void IFileSystem::GetEntryType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path = std::string(state.process->GetPointer<char>(request.inputBuf.at(0).address));

        auto type = backing->GetEntryType(path);

        if (type.has_value()) {
            response.Push(type.value());
        } else {
            response.Push<u32>(0);
            response.errorCode = constant::status::PathDoesNotExist;
        }
    }

    void IFileSystem::OpenFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path = std::string(state.process->GetPointer<char>(request.inputBuf.at(0).address));
        auto mode = request.Pop<vfs::Backing::Mode>();

        if (!backing->FileExists(path)) {
            response.errorCode = constant::status::PathDoesNotExist;
            return;
        }

        auto file = backing->OpenFile(path, mode);
        if (file == nullptr)
            response.errorCode = constant::status::GenericError;
        else
            manager.RegisterService(std::make_shared<IFile>(file, state, manager), session, response);
    }

    void IFileSystem::Commit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
