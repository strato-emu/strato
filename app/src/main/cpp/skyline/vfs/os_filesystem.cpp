// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include "os_backing.h"
#include "os_filesystem.h"

namespace skyline::vfs {
    OsFileSystem::OsFileSystem(const std::string &basePath) : FileSystem(), basePath(basePath.ends_with('/') ? basePath : basePath + '/') {
        if (!DirectoryExists(""))
            if (!CreateDirectory("", true))
                throw exception("Error creating the OS filesystem backing directory");
    }

    bool OsFileSystem::CreateFileImpl(const std::string &path, size_t size) {
        auto fullPath{basePath + path};

        // Create a directory that will hold the file
        CreateDirectory(path.substr(0, path.find_last_of('/')), true);
        int fd{open(fullPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)};
        if (fd < 0) {
            if (errno != ENOENT)
                throw exception("Failed to create file: {}", strerror(errno));
            else
                return false;
        }

        // Truncate the file to desired length
        int ret{ftruncate(fd, static_cast<off_t>(size))};
        close(fd);

        if (ret < 0)
            throw exception("Failed to resize created file: {}", strerror(errno));

        return true;
    }

    void OsFileSystem::DeleteFileImpl(const std::string &path) {
        auto fullPath{basePath + path};
        remove(fullPath.c_str());
    }

    void OsFileSystem::DeleteDirectoryImpl(const std::string &path) {
        auto fullPath{basePath + path};
        std::filesystem::remove_all(fullPath.c_str());
    }

    bool OsFileSystem::CreateDirectoryImpl(const std::string &path, bool parents) {
        auto fullPath{basePath + path + "/"};

        if (!parents) {
            int ret{mkdir(fullPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)};
            return ret == 0 || errno == EEXIST;
        }

        for (auto dir{fullPath.begin()}; dir != fullPath.end(); dir++) {
            auto nextDir{std::find(dir, fullPath.end(), '/')};
            auto nextPath{"/" + std::string(fullPath.begin(), nextDir)};

            int ret{mkdir(nextPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)};
            if (ret < 0 && errno != EEXIST && errno != EPERM)
                return false;

            dir = nextDir;
        }

        return true;
    }

    std::shared_ptr<Backing> OsFileSystem::OpenFileImpl(const std::string &path, Backing::Mode mode) {
        int fd{open((basePath + path).c_str(), (mode.read && mode.write) ? O_RDWR : (mode.write ? O_WRONLY : O_RDONLY))};
        if (fd < 0)
            throw exception("Failed to open file at '{}': {}", path, strerror(errno));

        return std::make_shared<OsBacking>(fd, true, mode);
    }

    std::optional<Directory::EntryType> OsFileSystem::GetEntryTypeImpl(const std::string &path) {
        auto fullPath{basePath + path};

        auto directory{opendir(fullPath.c_str())};
        if (directory) {
            closedir(directory);
            return Directory::EntryType::Directory;
        }

        if (access(fullPath.c_str(), F_OK) != -1)
            return Directory::EntryType::File;

        return std::nullopt;
    }

    std::shared_ptr<Directory> OsFileSystem::OpenDirectoryImpl(const std::string &path, Directory::ListMode listMode) {
        struct dirent *entry;
        auto directory{opendir((basePath + path).c_str())};
        if (!directory)
            return nullptr;
        else
            return std::make_shared<OsFileSystemDirectory>(basePath + path, listMode);
    }

    OsFileSystemDirectory::OsFileSystemDirectory(std::string path, Directory::ListMode listMode) : Directory(listMode), path(std::move(path)) {}

    std::vector<Directory::Entry> OsFileSystemDirectory::Read() {
        if (!listMode.file && !listMode.directory)
            return {};

        std::vector<Directory::Entry> outputEntries;

        struct dirent *entry;
        auto directory{opendir(path.c_str())};
        if (!directory)
            throw exception("Failed to open directory: {}, error: {}", path, strerror(errno));

        while ((entry = readdir(directory))) {
            struct stat entryInfo;
            if (stat((path + std::string(entry->d_name)).c_str(), &entryInfo))
                throw exception("Failed to stat directory entry: {}, error: {}", entry->d_name, strerror(errno));

            std::string name(entry->d_name);
            if (S_ISDIR(entryInfo.st_mode) && listMode.directory && (name != ".") && (name != "..")) {
                outputEntries.push_back(Directory::Entry{
                    .type = Directory::EntryType::Directory,
                    .name = std::string(entry->d_name),
                    .size = 0,
                });
            } else if (S_ISREG(entryInfo.st_mode) && listMode.file) {
                outputEntries.push_back(Directory::Entry{
                    .type = Directory::EntryType::File,
                    .name = std::string(entry->d_name),
                    .size = static_cast<size_t>(entryInfo.st_size),
                });
            }
        }
        closedir(directory);

        return outputEntries;
    }
}
