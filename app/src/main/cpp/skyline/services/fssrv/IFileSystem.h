// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/filesystem.h>
#include <services/serviceman.h>

namespace skyline::service::fssrv {
    /**
     * @brief IFileSystem is used to interact with a filesystem
     * @url https://switchbrew.org/wiki/Filesystem_services#IFileSystem
     */
    class IFileSystem : public BaseService {
      private:
        std::shared_ptr<vfs::FileSystem> backing;

      public:
        IFileSystem(std::shared_ptr<vfs::FileSystem> backing, const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Creates a file at the specified path in the filesystem
         */
        Result CreateFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Deletes a file at the specified path in the filesystem
         */
        Result DeleteFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Creates a directory at the specified path in the filesystem
         */
        Result CreateDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Deletes a directory at the specified path in the filesystem
         */
        Result DeleteDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Queries the DirectoryEntryType of the given path
         * @url https://switchbrew.org/wiki/Filesystem_services#GetEntryType
         */
        Result GetEntryType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an IFile handle for the requested path
         * @url https://switchbrew.org/wiki/Filesystem_services#OpenFile
         */
        Result OpenFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns an IDirectory handle for the requested path and mask (https://switchbrew.org/wiki/Filesystem_services#OpenDirectory)
         */
        Result OpenDirectory(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Commits all changes to the filesystem
         * @url https://switchbrew.org/wiki/Filesystem_services#Commit
         */
        Result Commit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the total free space of the filesystem
         * @url https://switchbrew.org/wiki/Filesystem_services#GetFreeSpaceSize
         */
        Result GetFreeSpaceSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IFileSystem, CreateFile),
            SFUNC(0x1, IFileSystem, DeleteFile),
            SFUNC(0x2, IFileSystem, CreateDirectory),
            SFUNC(0x3, IFileSystem, DeleteDirectory),
            SFUNC(0x7, IFileSystem, GetEntryType),
            SFUNC(0x8, IFileSystem, OpenFile),
            SFUNC(0x9, IFileSystem, OpenDirectory),
            SFUNC(0xA, IFileSystem, Commit),
            SFUNC(0xB, IFileSystem, GetFreeSpaceSize)
        )
    };
}
