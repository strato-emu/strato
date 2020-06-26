// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "os_backing.h"

namespace skyline::vfs {
    OsBacking::OsBacking(int fd) : Backing(), fd(fd) {
        struct stat fileInfo;

        if (fstat(fd, &fileInfo))
            throw exception("Failed to stat fd: {}", strerror(errno));

        size = fileInfo.st_size;
    }

    size_t OsBacking::Read(u8 *output, size_t offset, size_t size) {
        if (!mode.read)
            throw exception("Attempting to read a backing that is not readable");

        auto ret = pread64(fd, output, size, offset);

        if (ret < 0)
            throw exception("Failed to read from fd: {}", strerror(errno));

        return static_cast<size_t>(ret);
    }
}