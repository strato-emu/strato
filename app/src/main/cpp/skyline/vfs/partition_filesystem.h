// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include "filesystem.h"

namespace skyline::vfs {
    /**
     * @brief The PartitionFileSystem class abstracts a partition filesystem using the vfs::FileSystem api
     */
    class PartitionFileSystem : public FileSystem {
      private:
        /**
         * @brief This holds the header of the filesystem
         */
        struct FsHeader {
            u32 magic; //!< The filesystem magic: 'PFS0' or 'HFS0'
            u32 numFiles; //!< The number of files in the filesystem
            u32 stringTableSize; //!< The size of the filesystem's string table
            u32 _pad_;
        } header{};
        static_assert(sizeof(FsHeader) == 0x10);

        /**
         * @brief This holds a file entry in a partition filesystem
         */
        struct PartitionFileEntry {
            u64 offset; //!< The offset of the file in the backing
            u64 size; //!< The size of the file
            u32 stringTableOffset; //!< The offset of the file in the string table
            u32 _pad_;
        };
        static_assert(sizeof(PartitionFileEntry) == 0x18);

        /**
         * @brief This holds a file entry in a hashed filesystem
         */
        struct HashedFileEntry {
            PartitionFileEntry entry; //!< The base file entry
            u32 _pad_;
            std::array<u8, 0x20> hash; //!< The hash of the file
        };
        static_assert(sizeof(HashedFileEntry) == 0x40);

        bool hashed; //!< Whether the filesystem contains hash data
        size_t fileDataOffset; //!< The offset from the backing to the base of the file data
        std::shared_ptr<Backing> backing; //!< The backing file of the filesystem
        std::unordered_map<std::string, PartitionFileEntry> fileMap; //!< A map that maps file names to their corresponding entry

      public:
        PartitionFileSystem(std::shared_ptr<Backing> backing);

        std::shared_ptr<Backing> OpenFile(std::string path, Backing::Mode mode = {true, false, false});

        std::optional<Directory::EntryType> GetEntryType(std::string path);

        std::shared_ptr<Directory> OpenDirectory(std::string path, Directory::ListMode listMode);
    };

    /**
     * @brief The PartitionFileSystemDirectory provides access to the root directory of a partition filesystem
     */
    class PartitionFileSystemDirectory : public Directory {
      private:
        std::vector<Entry> fileList; //!< A list of every file in the PFS root directory

      public:
        PartitionFileSystemDirectory(const std::vector<Entry> &fileList, ListMode listMode);

        std::vector<Entry> Read();
    };
}
