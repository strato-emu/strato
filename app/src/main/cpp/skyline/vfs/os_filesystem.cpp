// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/stat.h>
#include <fcntl.h>

#include <dirent.h>
#include <unistd.h>
#include "os_backing.h"
#include "os_filesystem.h"

namespace skyline::vfs {
    OsFileSystem::OsFileSystem(std::string basePath) : FileSystem(), basePath(basePath) {
        if (!DirectoryExists(basePath))
            if (!CreateDirectory(basePath, true))
                throw exception("Error creating the OS filesystem backing directory");
    }

    bool OsFileSystem::CreateFile(std::string path, size_t size) {
        auto fullPath = basePath + path;

        // Create a directory that will hold the file
        CreateDirectory(fullPath.substr(0, fullPath.find_last_of('/')), true);
        int fd = open(fullPath.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            if (errno != ENOENT)
                throw exception("Failed to create file: {}", strerror(errno));
            else
                return false;
        }

        // Truncate the file to desired length
        int ret = ftruncate(fd, size);
        close(fd);

        if (ret < 0)
            throw exception("Failed to resize created file: {}", strerror(errno));

        return true;
    }

    bool OsFileSystem::CreateDirectory(std::string path, bool parents) {
        if (!parents) {
            int ret = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            return ret == 0 || errno == EEXIST;
        }

        for (auto dir = basePath.begin(); dir != basePath.end(); dir++) {
            auto nextDir = std::find(dir, basePath.end(), '/');
            auto nextPath = "/" + std::string(basePath.begin(), nextDir);

            int ret = mkdir(nextPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if (ret < 0 && errno != EEXIST && errno != EPERM)
                return false;

            dir = nextDir;
        }

        return true;
    }


    std::shared_ptr<Backing> OsFileSystem::OpenFile(std::string path, Backing::Mode mode) {
        if (!(mode.read || mode.write))
            throw exception("Cannot open a file that is neither readable or writable");

        int fd = open((basePath + path).c_str(), (mode.read && mode.write) ? O_RDWR : (mode.write ? O_WRONLY : O_RDONLY));
        if (fd < 0)
            throw exception("Failed to open file: {}", strerror(errno));

        return std::make_shared<OsBacking>(fd, true, mode);
    }

    std::optional<Directory::EntryType> OsFileSystem::GetEntryType(std::string path) {
        auto fullPath = basePath + path;

        auto directory = opendir(fullPath.c_str());
        if (directory) {
            closedir(directory);
            return Directory::EntryType::Directory;
        }

        if (access(fullPath.c_str(), F_OK) != -1)
            return Directory::EntryType::File;

        return std::nullopt;
    }
}
