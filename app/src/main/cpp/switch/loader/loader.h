#pragma once

#include <string>
#include "../os.h"

namespace lightSwitch::loader {
    class Loader {
    protected:
        std::string file_path; //!< The path to the ROM file
        std::ifstream file; //!< An input stream from the file

        /**
         * Read the file at a particular offset
         * @tparam T The type of object to write to
         * @param output The object to write to
         * @param offset The offset to read the file at
         * @param size The amount to read in bytes
         */
        template<typename T>
        void ReadOffset(T *output, uint32_t offset, size_t size) {
            file.seekg(offset, std::ios_base::beg);
            file.read(reinterpret_cast<char *>(output), size);
        }


    public:
        /**
         * @param file_path_ The path to the ROM file
         */
        Loader(std::string &file_path) : file_path(file_path), file(file_path, std::ios::binary | std::ios::beg) {}
    };
}
