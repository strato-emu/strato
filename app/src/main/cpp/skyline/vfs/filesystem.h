// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "backing.h"
#include "directory.h"

namespace skyline::vfs {
    /**
     * @brief The FileSystem class represents an abstract filesystem with child files and folders
     */
    class FileSystem {
      public:
        FileSystem() = default;

        /* Delete the move constructor to prevent multiple instances of the same filesystem */
        FileSystem(const FileSystem &) = delete;

        FileSystem &operator=(const FileSystem &) = delete;

        virtual ~FileSystem() = default;

        /**
         * @brief Opens a file from the specified path in the filesystem
         * @param path The path to the file
         * @param mode The mode to open the file with
         * @return A shared pointer to a Backing object of the file
         */
        virtual std::shared_ptr<Backing> OpenFile(std::string path, Backing::Mode mode = {true, false, false}) = 0;

        /**
         * @brief Checks if a given file exists in a filesystem
         * @param path The path to the file
         * @return A boolean containing whether the file exists
         */
        virtual bool FileExists(std::string path) = 0;

        /**
         * @brief Opens a directory from the specified path in the filesystem
         * @param path The path to the directory
         * @param listMode The list mode for the directory
         * @return A shared pointer to a Directory object of the directory
         */
        virtual std::shared_ptr<Directory> OpenDirectory(std::string path, Directory::ListMode listMode) = 0;
    };
}
