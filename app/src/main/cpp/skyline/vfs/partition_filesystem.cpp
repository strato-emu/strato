// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "region_backing.h"
#include "partition_filesystem.h"

namespace skyline::vfs {
    PartitionFileSystem::PartitionFileSystem(std::shared_ptr<Backing> backing) : FileSystem(), backing(backing) {
        header = backing->Read<FsHeader>();

        if (header.magic == util::MakeMagic<u32>("PFS0"))
            hashed = false;
        else if (header.magic == util::MakeMagic<u32>("HFS0"))
            hashed = true;
        else
            throw exception("Invalid filesystem magic: {}", header.magic);

        size_t entrySize{hashed ? sizeof(HashedFileEntry) : sizeof(PartitionFileEntry)};
        size_t stringTableOffset{sizeof(FsHeader) + (header.numFiles * entrySize)};
        fileDataOffset = stringTableOffset + header.stringTableSize;

        std::vector<char> stringTable(header.stringTableSize + 1);
        backing->Read(span(stringTable).first(header.stringTableSize).cast<u8>(), stringTableOffset);
        stringTable[header.stringTableSize] = 0;

        for (u32 entryOffset{sizeof(FsHeader)}; entryOffset < header.numFiles * entrySize; entryOffset += entrySize) {
            auto entry{backing->Read<PartitionFileEntry>(entryOffset)};

            std::string name(&stringTable[entry.stringTableOffset]);
            fileMap.emplace(name, std::move(entry));
        }
    }

    std::shared_ptr<Backing> PartitionFileSystem::OpenFile(const std::string &path, Backing::Mode mode) {
        try {
            auto &entry{fileMap.at(path)};
            return std::make_shared<RegionBacking>(backing, fileDataOffset + entry.offset, entry.size, mode);
        } catch (std::out_of_range &e) {
            return nullptr;
        }
    }

    std::optional<Directory::EntryType> PartitionFileSystem::GetEntryType(const std::string &path) {
        if (fileMap.count(path))
            return Directory::EntryType::File;

        return std::nullopt;
    }

    std::shared_ptr<Directory> PartitionFileSystem::OpenDirectory(const std::string &path, Directory::ListMode listMode) {
        // PFS doesn't have directories
        if (path != "")
            return nullptr;

        std::vector<Directory::Entry> fileList;
        for (const auto &file : fileMap)
            fileList.emplace_back(Directory::Entry{file.first, Directory::EntryType::File});

        return std::make_shared<PartitionFileSystemDirectory>(fileList, listMode);
    }

    PartitionFileSystemDirectory::PartitionFileSystemDirectory(const std::vector<Entry> &fileList, ListMode listMode) : Directory(listMode), fileList(fileList) {}

    std::vector<Directory::Entry> PartitionFileSystemDirectory::Read() {
        if (listMode.file)
            return fileList;
        else
            return std::vector<Entry>();
    }
}
