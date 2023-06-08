// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/stat.h>
#include <os.h>
#include "results.h"
#include "IFile.h"
#include "IDirectory.h"
#include "IFileSystem.h"

namespace skyline::service::fssrv {
    IFileSystem::IFileSystem(std::shared_ptr<vfs::FileSystem> backing, const DeviceState &state, ServiceManager &manager) : backing(std::move(backing)), BaseService(state, manager) {}

    Result IFileSystem::CreateFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path{request.inputBuf.at(0).as_string(true)};
        auto mode{request.Pop<u64>()};
        auto size{request.Pop<u32>()};

        return backing->CreateFile(path, size) ? Result{} : result::PathDoesNotExist;
    }

    Result IFileSystem::CreateDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path{request.inputBuf.at(0).as_string(true)};

        return backing->CreateDirectory(path, true) ? Result{} : result::PathDoesNotExist;
    }

    Result IFileSystem::GetEntryType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path(request.inputBuf.at(0).as_string(true));

        auto type{backing->GetEntryType(path)};

        if (type) {
            response.Push(static_cast<u32>(*type));
            return {};
        } else {
            response.Push<u32>(0);
            return result::PathDoesNotExist;
        }
    }

    Result IFileSystem::OpenFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path(request.inputBuf.at(0).as_string(true));
        auto mode{request.Pop<vfs::Backing::Mode>()};

        if (!backing->FileExists(path))
            return result::PathDoesNotExist;

        auto file{backing->OpenFileUnchecked(path, mode)};
        if (file == nullptr)
            return result::UnexpectedFailure;
        else
            manager.RegisterService(std::make_shared<IFile>(std::move(file), state, manager), session, response);

        return {};
    }

    Result IFileSystem::DeleteFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path(request.inputBuf.at(0).as_string(true));

        backing->DeleteFile(path);
        return {};
    }

    Result IFileSystem::DeleteDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path(request.inputBuf.at(0).as_string(true));

        backing->DeleteDirectory(path);
        return {};
    }

    Result IFileSystem::DeleteDirectoryRecursively(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path{request.inputBuf.at(0).as_string(true)};
        std::filesystem::remove_all(path);
        return {};
    }

    Result IFileSystem::OpenDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path(request.inputBuf.at(0).as_string(true));

        if (!path.ends_with("/"))
            path += "/";

        auto listMode{request.Pop<vfs::Directory::ListMode>()};
        auto directory{backing->OpenDirectory(path, listMode)};
        if (!directory)
            return result::PathDoesNotExist;

        manager.RegisterService(std::make_shared<IDirectory>(std::move(directory), backing, state, manager), session, response);
        return {};
    }

    Result IFileSystem::Commit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IFileSystem::GetFreeSpaceSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        //TODO: proper implementation for GetFreeSpaceSize
        response.Push<u64>(90000000);
        return {};
    }

    Result IFileSystem::GetTotalSpaceSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(90000000);
        return {};
    }

    Result IFileSystem::CleanDirectoryRecursively(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string path{request.inputBuf.at(0).as_string(true)};
        std::filesystem::remove_all(path);
        backing->CreateDirectory(path, true);
        return {};
    }

    Result IFileSystem::GetFileTimeStampRaw(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct FileTimeStampRaw {
            u64 created{};
            u64 accessed{};
            u64 modified{};
            u64 _pad_{};
        };

        std::string path{request.inputBuf.at(0).as_string(true)};
        struct stat fileStatus{};
        const auto statResult{stat((state.os->publicAppFilesPath + "/switch/sdmc/" + path).c_str(), &fileStatus)};
        FileTimeStampRaw fileTimeStampRaw{
            static_cast<u64>(fileStatus.st_ctim.tv_nsec),
            static_cast<u64>(fileStatus.st_atim.tv_nsec),
            static_cast<u64>(fileStatus.st_mtim.tv_nsec),
        };
        response.Push(fileTimeStampRaw);
        return {};
    }
}
