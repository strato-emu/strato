// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/account/IAccountServiceForApplication.h>
#include "IFileSystem.h"

namespace skyline::service::fssrv {
    enum class SaveDataSpaceId : u64 {
        System = 0, //!< Savedata should be stored in the EMMC system folder
        User = 1, //!< Savedata should be stored in the EMMC user folder
        SdSystem = 2, //!< Savedata should be stored in the SDCard system folder
        Temporary = 3, //!< Savedata should be stored in a temporary folder
        SdCache = 4, //!< Savedata should be stored in the SDCard system folder
        ProperSystem = 100, //!< Savedata should be stored in the system partition
    };

    enum class SaveDataType : u8 {
        System = 0, //!< System savedata
        Account = 1, //!< User game savedata
        Bcat = 2, //!< User BCAT savedata
        Device = 3, //!< Device-wide savedata
        Temporary = 4, //!< Temporary savedata
        Cache = 5, //!< Cache savedata
        SystemBcat = 6, //!< Device-wide BCAT savedata
    };

    enum class SaveDataRank : u8 {
        Primary,
        Secondary,
    };

    struct SaveDataAttribute {
        u64 programId; //!< The program ID to store the savedata contents under
        account::UserId userId; //!< The user ID of whom the applications savedata contents should be stored under
        u64 saveDataId; //!< The ID of the savedata
        SaveDataType type; //!< The type of savedata
        SaveDataRank rank; //!< The rank of the savedata
        u16 index; //!< The index of the savedata
        u8 _pad_[0x1A];
    };
    static_assert(sizeof(SaveDataAttribute) == 0x40);

    enum class StorageId : u8 {
        None = 0,
        Host = 1,
        GameCard = 2,
        NandSystem = 3,
        NandUser = 4,
        SdCard = 5,
    };

    /**
     * @brief IFileSystemProxy or fsp-srv is responsible for providing handles to file systems
     * @url https://switchbrew.org/wiki/Filesystem_services#fsp-srv
     */
    class IFileSystemProxy : public BaseService {
      public:
        u64 process{}; //!< The PID as set by SetCurrentProcess

        IFileSystemProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Sets the PID of the process using FS currently
         * @url https://switchbrew.org/wiki/Filesystem_services#SetCurrentProcess
         */
        Result SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to an instance of #IFileSystem
         * @url https://switchbrew.org/wiki/Filesystem_services#IFileSystem with type SDCard
         */
        Result OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetCacheStorageSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to an instance of #IFileSystem
         * @url https://switchbrew.org/wiki/Filesystem_services#IFileSystem for the requested save data area
         * @url https://switchbrew.org/wiki/Filesystem_services#OpenSaveDataFileSystem
         */
        Result OpenSaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a read-only instance of #IFileSystem
         * @url https://switchbrew.org/wiki/Filesystem_services#IFileSystem for the requested save data area
         * @url https://switchbrew.org/wiki/Filesystem_services#OpenReadOnlySaveDataFileSystem
         */
        Result OpenReadOnlySaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to an instance of #IStorage
         * @url https://switchbrew.org/wiki/Filesystem_services#IStorage for the application's data storage
         */
        Result OpenDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to an instance of #IStorage
         * @url https://switchbrew.org/wiki/Filesystem_services#OpenDataStorageByDataId
         */
        Result OpenDataStorageByDataId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result OpenPatchDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
          * @brief Returns the filesystem log access mode
          * @url https://switchbrew.org/wiki/Filesystem_services#GetGlobalAccessLogMode
          */
        Result GetGlobalAccessLogMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
          * @url https://switchbrew.org/wiki/Filesystem_services#OpenMultiCommitManager
          */
        Result OpenMultiCommitManager(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IFileSystemProxy, SetCurrentProcess),
            SFUNC(0x12, IFileSystemProxy, OpenSdCardFileSystem),
            SFUNC(0x22, IFileSystemProxy, GetCacheStorageSize),
            SFUNC(0x33, IFileSystemProxy, OpenSaveDataFileSystem),
            SFUNC(0x35, IFileSystemProxy, OpenReadOnlySaveDataFileSystem),
            SFUNC(0xC8, IFileSystemProxy, OpenDataStorageByCurrentProcess),
            SFUNC(0xCA, IFileSystemProxy, OpenDataStorageByDataId),
            SFUNC(0xCB, IFileSystemProxy, OpenPatchDataStorageByCurrentProcess),
            SFUNC(0x3ED, IFileSystemProxy, GetGlobalAccessLogMode),
            SFUNC(0x4B0, IFileSystemProxy, OpenMultiCommitManager)
        )
    };
}
