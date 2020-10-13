// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <vfs/os_filesystem.h>
#include <loader/loader.h>
#include "results.h"
#include "IStorage.h"
#include "IFileSystemProxy.h"

namespace skyline::service::fssrv {
    IFileSystemProxy::IFileSystemProxy(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IFileSystemProxy::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = request.Pop<pid_t>();
        return {};
    }

    Result IFileSystemProxy::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(std::make_shared<vfs::OsFileSystem>(state.os->appFilesPath + "/switch/sdmc/"), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenSaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto spaceId{request.Pop<SaveDataSpaceId>()};
        auto attribute{request.Pop<SaveDataAttribute>()};

        if (attribute.programId == 0)
            attribute.programId = state.loader->nacp->nacpContents.saveDataOwnerId;

        std::string saveDataPath{[spaceId, &attribute]() {
            std::string spaceIdStr{[spaceId]() {
                switch (spaceId) {
                    case SaveDataSpaceId::System:
                        return "/nand/system";
                    case SaveDataSpaceId::User:
                        return "/nand/user";
                    case SaveDataSpaceId::Temporary:
                        return "/nand/temp";
                    default:
                        throw exception("Unsupported savedata ID: {}", spaceId);
                };
            }()};

            switch (attribute.type) {
                case SaveDataType::System:
                    return fmt::format("{}/save/{:016X}/{:016X}{:016X}/", spaceIdStr, attribute.saveDataId, attribute.userId.lower, attribute.userId.upper);
                case SaveDataType::Account:
                case SaveDataType::Device:
                    return fmt::format("{}/save/{:016X}/{:016X}{:016X}/{:016X}/", spaceIdStr, 0, attribute.userId.lower, attribute.userId.upper, attribute.programId);
                case SaveDataType::Temporary:
                    return fmt::format("{}/{:016X}/{:016X}{:016X}/{:016X}/", spaceIdStr, 0, attribute.userId.lower, attribute.userId.upper, attribute.programId);
                case SaveDataType::Cache:
                    return fmt::format("{}/save/cache/{:016X}/", spaceIdStr, attribute.programId);
                default:
                    throw exception("Unsupported savedata type: {}", attribute.type);
            };
        }()};

        manager.RegisterService(std::make_shared<IFileSystem>(std::make_shared<vfs::OsFileSystem>(state.os->appFilesPath + "/switch" + saveDataPath), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!state.loader->romFs)
            return result::NoRomFsAvailable;

        manager.RegisterService(std::make_shared<IStorage>(state.loader->romFs, state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::GetGlobalAccessLogMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }
}
