// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <vfs/os_filesystem.h>
#include <vfs/nca.h>
#include <loader/loader.h>
#include "results.h"
#include "IStorage.h"
#include "IMultiCommitManager.h"
#include "IFileSystemProxy.h"
#include "ISaveDataInfoReader.h"
#include "vfs/patch_manager.h"

namespace skyline::service::fssrv {
    IFileSystemProxy::IFileSystemProxy(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IFileSystemProxy::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = request.Pop<u64>();
        return {};
    }

    Result IFileSystemProxy::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "/switch/sdmc/"), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::GetCacheStorageSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(0);
        response.Push<u64>(0);
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
                }
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
            }
        }()};

        manager.RegisterService(std::make_shared<IFileSystem>(std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "/switch" + saveDataPath), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenReadOnlySaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Forward to OpenSaveDataFileSystem for now.
        // TODO: This should wrap the underlying filesystem with nn::fs::ReadOnlyFileSystem.
        return OpenSaveDataFileSystem(session, request, response);
    }

    Result IFileSystemProxy::OpenSaveDataInfoReader(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISaveDataInfoReader), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenSaveDataInfoReaderBySaveDataSpaceId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISaveDataInfoReader), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenSaveDataInfoReaderOnlyCacheStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISaveDataInfoReader), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {

        if (state.updateLoader) {
            auto patchManager{std::make_shared<vfs::PatchManager>()};
            auto romFs{patchManager->PatchRomFS(state, state.updateLoader->programNca, state.loader->programNca->ivfcOffset)};
            manager.RegisterService(std::make_shared<IStorage>(romFs, state, manager), session, response);
        } else {
            if (!state.loader->romFs)
                return result::NoRomFsAvailable;

            manager.RegisterService(std::make_shared<IStorage>(state.loader->romFs, state, manager), session, response);
        }
        return {};
    }

    Result IFileSystemProxy::OpenDataStorageByDataId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto storageId{request.Pop<StorageId>()};
        request.Skip<std::array<u8, 7>>(); // 7-bytes padding
        auto dataId{request.Pop<u64>()};
        auto patchManager{std::make_shared<vfs::PatchManager>()};

        // Try load DLC first
        for (const auto &dlc : state.dlcLoaders) {
            if (dlc->cnmt->header.id == dataId) {
                auto romFs{patchManager->PatchRomFS(state, dlc->publicNca, state.loader->programNca->ivfcOffset)};
                manager.RegisterService(std::make_shared<IStorage>(romFs, state, manager), session, response);
                return {};
            }
        }

        auto systemArchivesFileSystem{std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "/switch/nand/system/Contents/registered/")};
        auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};
        auto keyStore{std::make_shared<skyline::crypto::KeyStore>(state.os->privateAppFilesPath + "keys")};

        for (const auto &entry : systemArchives->Read()) {
            std::shared_ptr<vfs::Backing> backing{systemArchivesFileSystem->OpenFile(entry.name)};
            auto nca{vfs::NCA(backing, keyStore)};

            if (nca.header.titleId == dataId && nca.romFs != nullptr) {
                manager.RegisterService(std::make_shared<IStorage>(nca.romFs, state, manager), session, response);
                return {};
            }
        }

        auto romFs{std::make_shared<IStorage>(state.os->assetFileSystem->OpenFile(fmt::format("romfs/{:016X}", dataId)), state, manager)};

        manager.RegisterService(romFs, session, response);
        return {};
    }

    Result IFileSystemProxy::OpenPatchDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::EntityNotFound;
    }

    Result IFileSystemProxy::GetGlobalAccessLogMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IFileSystemProxy::OpenMultiCommitManager(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IMultiCommitManager), session, response);
        return {};
    }
}
