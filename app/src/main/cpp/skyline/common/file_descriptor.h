// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <unistd.h>
#include "base.h"

namespace skyline {
    /**
     * @brief A RAII wrapper around Linux file descriptors which automatically closes the file descriptor when it goes out of scope and duplicates them on copies
     * @note This class should **always** be moved rather than copied where possible to avoid a system call for duplicating file descriptors
     */
    class FileDescriptor {
      private:
        int fd;

      public:
        FileDescriptor() : fd(-1) {}

        FileDescriptor(int fd) : fd(fd) {}

        FileDescriptor &operator=(int newFd) {
            if (fd != -1)
                close(fd);
            fd = newFd;
            return *this;
        }

        FileDescriptor(const FileDescriptor &other) : fd(dup(other.fd)) {
            if (fd == -1)
                throw exception("Failed to duplicate file descriptor: {}", strerror(errno));
        }

        FileDescriptor &operator=(const FileDescriptor &other) {
            if (fd != -1)
                close(fd);
            fd = dup(other.fd);
            if (fd == -1)
                throw exception("Failed to duplicate file descriptor: {}", strerror(errno));
            return *this;
        }

        FileDescriptor(FileDescriptor &&other) : fd(other.fd) {
            other.fd = -1;
        }

        FileDescriptor &operator=(FileDescriptor &&other) {
            if (fd != -1)
                close(fd);
            fd = other.fd;
            other.fd = -1;
            return *this;
        }

        ~FileDescriptor() {
            if (fd != -1)
                close(fd);
        }

        operator int() const {
            return fd;
        }

        int operator*() const {
            return fd;
        }
    };
}
