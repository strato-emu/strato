// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <services/account/IAccountServiceForApplication.h>
#include "IFileSystem.h"

namespace skyline::service::fssrv {
    /**
     * @brief This enumerates the possible locations savedata can be stored in
     */
    enum class SaveDataSpaceId : u64 {
        System = 0, //!< Savedata should be stored in the EMMC system folder
        User = 1, //!< Savedata should be stored in the EMMC user folder
        SdSystem = 2, //!< Savedata should be stored in the SDCard system folder
        Temporary = 3, //!< Savedata should be stored in a temporary folder
        SdCache = 4, //!< Savedata should be stored in the SDCard system folder
        ProperSystem = 100, //!< Savedata should be stored in the system partition
    };

    /**
     * @brief This enumerates the types of savedata
     */
    enum class SaveDataType : u8 {
        System = 0, //!< This is system savedata
        Account = 1, //!< This is user game savedata
        Bcat = 2, //!< This is user bcat savedata
        Device = 3, //!< This is device-wide savedata
        Temporary = 4, //!< This is temporary savedata
        Cache = 5, //!< This is cache savedata
        SystemBcat = 6, //!< This is device-wide bcat savedata
    };

    /**
     * @brief This enumerates the ranks of savedata
     */
    enum class SaveDataRank : u8 {
        Primary, //!< This is primary savedata
        Secondary, //!< This is secondary savedata
    };

    /**
     * @brief This stores the attributes of a savedata entry
     */
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

    /**
     * @brief IFileSystemProxy or fsp-srv is responsible for providing handles to file systems (https://switchbrew.org/wiki/Filesystem_services#fsp-srv)
     */
    class IFileSystemProxy : public BaseService {
      public:
        pid_t process{}; //!< This holds the PID set by SetCurrentProcess

        IFileSystemProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This sets the PID of the process using FS currently (https://switchbrew.org/wiki/Filesystem_services#SetCurrentProcess)
         */
        Result SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to an instance of #IFileSystem (https://switchbrew.org/wiki/Filesystem_services#IFileSystem) with type SDCard
         */
        Result OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to an instance of #IFileSystem (https://switchbrew.org/wiki/Filesystem_services#IFileSystem) for the requested save data area
         */
        Result OpenSaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to an instance of #IStorage (https://switchbrew.org/wiki/Filesystem_services#IStorage) for the application's data storage
         */
        Result OpenDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
          * @brief This returns the filesystem log access mode (https://switchbrew.org/wiki/Filesystem_services#GetGlobalAccessLogMode)
          */
        Result GetGlobalAccessLogMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
