// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <unistd.h>
#include <os.h>
#include <kernel/types/KProcess.h>

namespace skyline::loader {
    class Loader {
      protected:
        int fd; //!< An FD to the ROM file

        /**
         * @brief Read the file at a particular offset
         * @tparam T The type of object to write to
         * @param output The object to write to
         * @param offset The offset to read the file at
         * @param size The amount to read in bytes
         */
        template<typename T>
        inline void ReadOffset(T *output, u64 offset, size_t size) {
            pread64(fd, output, size, offset);
        }

      public:
        /**
         * @param filePath The path to the ROM file
         */
        Loader(int fd) : fd(fd) {}

        /**
         * This loads in the data of the main process
         * @param process The process to load in the data
         * @param state The state of the device
         */
        virtual void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) = 0;
    };
}
