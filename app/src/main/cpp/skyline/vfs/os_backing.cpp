// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "os_backing.h"

namespace skyline::vfs {
    OsBacking::OsBacking(int fd, bool closable, Mode mode) : Backing(mode), fd(fd), closable(closable) {
        struct stat fileInfo;
        if (fstat(fd, &fileInfo))
            throw exception("Failed to stat fd: {}", strerror(errno));

        size = fileInfo.st_size;
    }

    OsBacking::~OsBacking() {
        if (closable)
            close(fd);
    }

    size_t OsBacking::ReadImpl(span<u8> output, size_t offset) {
        auto ret{pread64(fd, output.data(), output.size(), offset)};
        if (ret < 0)
            throw exception("Failed to read from fd: {}", strerror(errno));

        return static_cast<size_t>(ret);
    }

    size_t OsBacking::WriteImpl(span<u8> input, size_t offset) {
        auto ret{pwrite64(fd, input.data(), input.size(), offset)};
        if (ret < 0)
            throw exception("Failed to write to fd: {}", strerror(errno));

        return static_cast<size_t>(ret);
    }

    void OsBacking::ResizeImpl(size_t pSize) {
        int ret{ftruncate(fd, pSize)};
        if (ret < 0)
            throw exception("Failed to resize file: {}", strerror(errno));

        size = pSize;
    }
}
