// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "region_backing.h"
#include "rom_filesystem.h"

namespace skyline::vfs {
    RomFileSystem::RomFileSystem(std::shared_ptr<Backing> pBacking) : FileSystem(), backing(std::move(pBacking)) {
        header = backing->Read<RomFsHeader>();
        TraverseDirectory(0, "");
    }

    void RomFileSystem::TraverseFiles(u32 offset, const std::string &path) {
        RomFsFileEntry entry;
        do {
            entry = backing->Read<RomFsFileEntry>(header.fileMetaTableOffset + offset);

            if (entry.nameSize) {
                std::vector<char> name(entry.nameSize);
                backing->Read(span(name), header.fileMetaTableOffset + offset + sizeof(RomFsFileEntry));

                std::string fullPath{path + (path.empty() ? "" : "/") + std::string(name.data(), entry.nameSize)};
                fileMap.emplace(fullPath, entry);
            }

            offset = entry.siblingOffset;
        } while (offset != constant::RomFsEmptyEntry);
    }

    void RomFileSystem::TraverseDirectory(u32 offset, const std::string &path) {
        RomFsDirectoryEntry entry;
        do {
            entry = backing->Read<RomFsDirectoryEntry>(header.dirMetaTableOffset + offset);

            std::string childPath(path);
            if (entry.nameSize) {
                std::vector<char> name(entry.nameSize);
                backing->Read(span(name), header.dirMetaTableOffset + offset + sizeof(RomFsDirectoryEntry));
                childPath = path + (path.empty() ? "" : "/") + std::string(name.data(), entry.nameSize);
            }

            directoryMap.emplace(childPath, entry);

            if (entry.fileOffset != constant::RomFsEmptyEntry)
                TraverseFiles(entry.fileOffset, childPath);

            if (entry.childOffset != constant::RomFsEmptyEntry)
                TraverseDirectory(entry.childOffset, childPath);

            offset = entry.siblingOffset;
        } while (entry.siblingOffset != constant::RomFsEmptyEntry);
    }

    std::shared_ptr<Backing> RomFileSystem::OpenFileImpl(const std::string &path, Backing::Mode mode) {
        try {
            const auto &entry{fileMap.at(path)};
            return std::make_shared<RegionBacking>(backing, header.dataOffset + entry.offset, entry.size, mode);
        } catch (std::out_of_range &e) {
            return nullptr;
        }
    }

    std::optional<Directory::EntryType> RomFileSystem::GetEntryTypeImpl(const std::string &path) {
        if (fileMap.count(path))
            return Directory::EntryType::File;
        else if (directoryMap.count(path))
            return Directory::EntryType::Directory;

        return std::nullopt;
    }

    std::shared_ptr<Directory> RomFileSystem::OpenDirectoryImpl(const std::string &path, Directory::ListMode listMode) {
        try {
            auto &entry{directoryMap.at(path)};
            return std::make_shared<RomFileSystemDirectory>(backing, header, entry, listMode);
        } catch (std::out_of_range &e) {
            return nullptr;
        }
    }

    RomFileSystemDirectory::RomFileSystemDirectory(std::shared_ptr<Backing> backing, const RomFileSystem::RomFsHeader &header, const RomFileSystem::RomFsDirectoryEntry &ownEntry, ListMode listMode) : Directory(listMode), backing(std::move(backing)), header(header), ownEntry(ownEntry) {}

    std::vector<RomFileSystemDirectory::Entry> RomFileSystemDirectory::Read() {
        std::vector<Entry> contents;

        if (listMode.file) {
            RomFileSystem::RomFsFileEntry romFsFileEntry;
            u32 offset{ownEntry.fileOffset};

            do {
                romFsFileEntry = backing->Read<RomFileSystem::RomFsFileEntry>(header.fileMetaTableOffset + offset);

                if (romFsFileEntry.nameSize) {
                    std::vector<char> name(romFsFileEntry.nameSize);
                    backing->Read(span(name), header.fileMetaTableOffset + offset + sizeof(RomFileSystem::RomFsFileEntry));

                    contents.emplace_back(Entry{std::string(name.data(), romFsFileEntry.nameSize), EntryType::File, romFsFileEntry.size});
                }

                offset = romFsFileEntry.siblingOffset;
            } while (offset != constant::RomFsEmptyEntry);
        }

        if (listMode.directory) {
            RomFileSystem::RomFsDirectoryEntry romFsDirectoryEntry;
            u32 offset{ownEntry.childOffset};

            do {
                romFsDirectoryEntry = backing->Read<RomFileSystem::RomFsDirectoryEntry>(header.dirMetaTableOffset + offset);

                if (romFsDirectoryEntry.nameSize) {
                    std::vector<char> name(romFsDirectoryEntry.nameSize);
                    backing->Read(span(name), header.dirMetaTableOffset + offset + sizeof(RomFileSystem::RomFsDirectoryEntry));

                    contents.emplace_back(Entry{std::string(name.data(), romFsDirectoryEntry.nameSize), EntryType::Directory});
                }

                offset = romFsDirectoryEntry.siblingOffset;
            } while (offset != constant::RomFsEmptyEntry);
        }

        return contents;
    }
}
