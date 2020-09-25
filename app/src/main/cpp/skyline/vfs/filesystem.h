// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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
         * @brief Creates a file in the filesystem with the requested size
         * @param path The path where the file should be created
         * @param size The size of the file to create
         * @return Whether creating the file succeeded
         */
        virtual bool CreateFile(const std::string &path, size_t size) {
            throw exception("This filesystem does not support creating files");
        };

        /**
         * @brief Creates a directory in the filesystem
         * @param path The path to where the directory should be created
         * @param parents Whether all parent directories in the given path should be created
         * @return Whether creating the directory succeeded
         */
        virtual bool CreateDirectory(const std::string &path, bool parents) {
            throw exception("This filesystem does not support creating directories");
        };

        /**
         * @brief Opens a file from the specified path in the filesystem
         * @param path The path to the file
         * @param mode The mode to open the file with
         * @return A shared pointer to a Backing object of the file
         */
        virtual std::shared_ptr<Backing> OpenFile(const std::string &path, Backing::Mode mode = {true, false, false}) = 0;

        /**
         * @brief Queries the type of the entry given by path
         * @param path The path to the entry
         * @return The type of the entry, if present
         */
        virtual std::optional<Directory::EntryType> GetEntryType(const std::string &path) = 0;

        /**
         * @brief Checks if a given file exists in a filesystem
         * @param path The path to the file
         * @return Whether the file exists
         */
        inline bool FileExists(const std::string &path) {
            auto entry = GetEntryType(path);
            return entry && *entry == Directory::EntryType::File;
        }

        /**
         * @brief Checks if a given directory exists in a filesystem
         * @param path The path to the directory
         * @return Whether the directory exists
         */
        inline bool DirectoryExists(const std::string &path) {
            auto entry = GetEntryType(path);
            return entry && *entry == Directory::EntryType::Directory;
        }

        /**
         * @brief Opens a directory from the specified path in the filesystem
         * @param path The path to the directory
         * @param listMode The list mode for the directory
         * @return A shared pointer to a Directory object of the directory
         */
        virtual std::shared_ptr<Directory> OpenDirectory(const std::string &path, Directory::ListMode listMode) {
            throw exception("This filesystem does not support opening directories");
        };
    };
}
