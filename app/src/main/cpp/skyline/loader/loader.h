#pragma once

#include <os.h>
#include <kernel/types/KProcess.h>
#include <unistd.h>

namespace skyline::loader {
    class Loader {
      protected:
        const int romFd; //!< An FD to the ROM file

        /**
         * @brief Read the file at a particular offset
         * @tparam T The type of object to write to
         * @param output The object to write to
         * @param offset The offset to read the file at
         * @param size The amount to read in bytes
         */
        template<typename T>
        inline void ReadOffset(T *output, u64 offset, size_t size) {
            pread64(romFd, output, size, offset);
        }

      public:
        /**
         * @param filePath The path to the ROM file
         */
        Loader(const int romFd) : romFd(romFd) {}

        /**
         * This loads in the data of the main process
         * @param process The process to load in the data
         * @param state The state of the device
         */
        virtual void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) = 0;
    };
}
