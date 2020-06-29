// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "region_backing.h"
#include "rom_filesystem.h"

namespace skyline::vfs {
    RomFileSystem::RomFileSystem(std::shared_ptr <Backing> backing) : FileSystem(), backing(backing) {
        backing->Read(&header);

        TraverseDirectory(0, "");
    }

    void RomFileSystem::TraverseFiles(u32 offset, std::string path) {
        RomFsFileEntry entry{};

        do {
            backing->Read(&entry, header.fileMetaTableOffset + offset);

            if (entry.nameSize) {
                std::vector<char> name(entry.nameSize);
                backing->Read(name.data(), header.fileMetaTableOffset + offset + sizeof(RomFsFileEntry), entry.nameSize);

                std::string fullPath = path + (path.empty() ? "" : "/") + std::string(name.data(), entry.nameSize);
                fileMap.emplace(fullPath, entry);
            }

            offset = entry.siblingOffset;
        } while (offset != constant::RomFsEmptyEntry);
    }

    void RomFileSystem::TraverseDirectory(u32 offset, std::string path) {
        RomFsDirectoryEntry entry{};
        backing->Read(&entry, header.dirMetaTableOffset + offset);

        std::string childPath = path;
        if (entry.nameSize) {
            std::vector<char> name(entry.nameSize);
            backing->Read(name.data(), header.dirMetaTableOffset + offset + sizeof(RomFsDirectoryEntry), entry.nameSize);
            childPath = path + (path.empty() ? "" : "/") + std::string(name.data(), entry.nameSize);
        }

        directoryMap.emplace(childPath, entry);

        if (entry.fileOffset != constant::RomFsEmptyEntry)
            TraverseFiles(entry.fileOffset, childPath);

        if (entry.childOffset != constant::RomFsEmptyEntry)
            TraverseDirectory(entry.childOffset, childPath);

        if (entry.siblingOffset != constant::RomFsEmptyEntry)
            TraverseDirectory(entry.siblingOffset, path);
    }

    std::shared_ptr <Backing> RomFileSystem::OpenFile(std::string path, Backing::Mode mode) {
        try {
            const auto &entry = fileMap.at(path);
            return std::make_shared<RegionBacking>(backing, header.dataOffset + entry.offset, entry.size, mode);
        } catch (std::out_of_range &e) {
            return nullptr;
        }
    }

    bool RomFileSystem::FileExists(std::string path) {
        return fileMap.count(path);
    }

    std::shared_ptr <Directory> RomFileSystem::OpenDirectory(std::string path, Directory::ListMode listMode) {
        try {
            auto &entry = directoryMap.at(path);
            return std::make_shared<RomFileSystemDirectory>(backing, header, entry, listMode);
        } catch (std::out_of_range &e) {
            return nullptr;
        }
    }

    RomFileSystemDirectory::RomFileSystemDirectory(const std::shared_ptr <Backing> &backing, const RomFileSystem::RomFsHeader &header, const RomFileSystem::RomFsDirectoryEntry &ownEntry, ListMode listMode) : Directory(listMode), backing(backing), header(header), ownEntry(ownEntry) {}

    std::vector <RomFileSystemDirectory::Entry> RomFileSystemDirectory::Read() {
        std::vector <Entry> contents;

        if (listMode.file) {
            RomFileSystem::RomFsFileEntry romFsFileEntry;
            u32 offset = ownEntry.fileOffset;

            do {
                backing->Read(&romFsFileEntry, header.fileMetaTableOffset + offset);

                if (romFsFileEntry.nameSize) {
                    std::vector<char> name(romFsFileEntry.nameSize);
                    backing->Read(name.data(), header.fileMetaTableOffset + offset + sizeof(RomFileSystem::RomFsFileEntry), romFsFileEntry.nameSize);

                    contents.emplace_back(Entry{std::string(name.data(), romFsFileEntry.nameSize), EntryType::File});
                }

                offset = romFsFileEntry.siblingOffset;
            } while (offset != constant::RomFsEmptyEntry);
        }

        if (listMode.directory) {
            RomFileSystem::RomFsDirectoryEntry romFsDirectoryEntry;
            u32 offset = ownEntry.childOffset;

            do {
                backing->Read(&romFsDirectoryEntry, header.dirMetaTableOffset + offset);

                if (romFsDirectoryEntry.nameSize) {
                    std::vector<char> name(romFsDirectoryEntry.nameSize);
                    backing->Read(name.data(), header.dirMetaTableOffset + offset + sizeof(RomFileSystem::RomFsDirectoryEntry), romFsDirectoryEntry.nameSize);

                    contents.emplace_back(Entry{std::string(name.data(), romFsDirectoryEntry.nameSize), EntryType::Directory});
                }

                offset = romFsDirectoryEntry.siblingOffset;
            } while (offset != constant::RomFsEmptyEntry);
        }

        return contents;
    }
}