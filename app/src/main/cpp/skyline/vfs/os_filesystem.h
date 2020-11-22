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

      public:
        OsFileSystem(const std::string &basePath);

        bool CreateFile(const std::string &path, size_t size);

        bool CreateDirectory(const std::string &path, bool parents);

        std::shared_ptr<Backing> OpenFile(const std::string &path, Backing::Mode mode = {true, false, false});

        std::optional<Directory::EntryType> GetEntryType(const std::string &path);

        std::shared_ptr<Directory> OpenDirectory(const std::string &path, Directory::ListMode listMode);
    };

    /**
     * @brief OsFileSystemDirectory abstracts access to a native linux directory through the VFS APIs
     */
    class OsFileSystemDirectory : public Directory {
      private:
        std::string path;

      public:
        OsFileSystemDirectory(const std::string &path, ListMode listMode);

        std::vector<Entry> Read();
    };
}
