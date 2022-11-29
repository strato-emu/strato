// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "filesystem.h"

namespace skyline::vfs {
    /**
     * @brief The OsFileSystem class abstracts an OS folder with the vfs::FileSystem api
     */
    class OsFileSystem : public FileSystem {
      private:
        std::string basePath; //!< The base path for filesystem operations

      protected:
        bool CreateFileImpl(const std::string &path, size_t size) override;

        void DeleteFileImpl(const std::string &path) override;

        void DeleteDirectoryImpl(const std::string &path) override;

        bool CreateDirectoryImpl(const std::string &path, bool parents) override;

        std::shared_ptr<Backing> OpenFileImpl(const std::string &path, Backing::Mode mode) override;

        std::optional<Directory::EntryType> GetEntryTypeImpl(const std::string &path) override;

        std::shared_ptr<Directory> OpenDirectoryImpl(const std::string &path, Directory::ListMode listMode) override;

      public:
        OsFileSystem(const std::string &basePath);
    };

    /**
     * @brief OsFileSystemDirectory abstracts access to a native linux directory through the VFS APIs
     */
    class OsFileSystemDirectory : public Directory {
      private:
        std::string path;

      public:
        OsFileSystemDirectory(std::string path, ListMode listMode);

        std::vector<Entry> Read();
    };
}
