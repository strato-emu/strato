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
        OsFileSystem(std::string basePath);

        bool CreateFile(std::string path, size_t size);

        bool CreateDirectory(std::string path, bool parents);

        std::shared_ptr<Backing> OpenFile(std::string path, Backing::Mode mode = {true, false, false});

        std::optional<Directory::EntryType> GetEntryType(std::string path);
    };
}
