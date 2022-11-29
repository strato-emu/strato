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
      protected:
        virtual bool CreateFileImpl(const std::string &path, size_t size) {
            throw exception("This filesystem does not support creating files");
        };

        virtual void DeleteFileImpl(const std::string &path) {
            throw exception("This filesystem does not support deleting files");
        }

        virtual void DeleteDirectoryImpl(const std::string &path) {
            throw exception("This filesystem does not support deleting directories");
        }

        virtual bool CreateDirectoryImpl(const std::string &path, bool parents) {
            throw exception("This filesystem does not support creating directories");
        };

        virtual std::shared_ptr<Backing> OpenFileImpl(const std::string &path, Backing::Mode mode) = 0;

        virtual std::optional<Directory::EntryType> GetEntryTypeImpl(const std::string &path) = 0;

        virtual std::shared_ptr<Directory> OpenDirectoryImpl(const std::string &path, Directory::ListMode listMode) {
            throw exception("This filesystem does not support opening directories");
        };

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
        bool CreateFile(const std::string &path, size_t size) {
            return CreateFileImpl(path, size);
        }

        void DeleteFile(const std::string &path) {
            DeleteFileImpl(path);
        }

        void DeleteDirectory(const std::string &path) {
            DeleteDirectoryImpl(path);
        }

        /**
         * @brief Creates a directory in the filesystem
         * @param path The path to where the directory should be created
         * @param parents Whether all parent directories in the given path should be created
         * @return Whether creating the directory succeeded
         */
        bool CreateDirectory(const std::string &path, bool parents) {
            return CreateDirectoryImpl(path, parents);
        };

        /**
         * @brief Opens a file from the specified path in the filesystem
         * @param path The path to the file
         * @param mode The mode to open the file with
         * @return A shared pointer to a Backing object of the file (may be nullptr)
         */
        std::shared_ptr<Backing> OpenFileUnchecked(const std::string &path, Backing::Mode mode = {true, false, false}) {
            if (!mode.write && !mode.read)
                throw exception("Cannot open a file with a mode that is neither readable nor writable");

            return OpenFileImpl(path, mode);
        }

        /**
         * @brief Opens a file from the specified path in the filesystem and throws an exception if opening fails
         * @param path The path to the file
         * @param mode The mode to open the file with
         * @return A shared pointer to a Backing object of the file
         */
        std::shared_ptr<Backing> OpenFile(const std::string &path, Backing::Mode mode = {true, false, false}) {
            auto file{OpenFileUnchecked(path, mode)};
            if (file == nullptr)
                throw exception("Failed to open file: {}", path);

            return file;
        }

        /**
         * @brief Queries the type of the entry given by path
         * @param path The path to the entry
         * @return The type of the entry, if present
         */
        std::optional<Directory::EntryType> GetEntryType(const std::string &path) {
            return GetEntryTypeImpl(path);
        }

        /**
         * @brief Checks if a given file exists in a filesystem
         * @param path The path to the file
         * @return Whether the file exists
         */
        bool FileExists(const std::string &path) {
            auto entry{GetEntryType(path)};
            return entry && *entry == Directory::EntryType::File;
        }

        /**
         * @brief Checks if a given directory exists in a filesystem
         * @param path The path to the directory
         * @return Whether the directory exists
         */
        bool DirectoryExists(const std::string &path) {
            auto entry{GetEntryType(path)};
            return entry && *entry == Directory::EntryType::Directory;
        }

        /**
         * @brief Opens a directory from the specified path in the filesystem
         * @param path The path to the directory
         * @param listMode The list mode for the directory
         * @return A shared pointer to a Directory object of the directory (may be nullptr)
         */
        std::shared_ptr<Directory> OpenDirectoryUnchecked(const std::string &path, Directory::ListMode listMode = {true, true}) {
            if (!listMode.raw)
                throw exception("Cannot open a directory with an empty listMode");

            return OpenDirectoryImpl(path, listMode);
        };

        /**
         * @brief Opens a directory from the specified path in the filesystem and throws an exception if opening fails
         * @param path The path to the directory
         * @param listMode The list mode for the directory
         * @return A shared pointer to a Directory object of the directory
         */
        std::shared_ptr<Directory> OpenDirectory(const std::string &path, Directory::ListMode listMode = {true, true}) {
            return OpenDirectoryUnchecked(path, listMode);
        };
    };
}
