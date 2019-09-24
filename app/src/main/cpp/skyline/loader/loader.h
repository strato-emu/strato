#pragma once

#include <string>
#include "../os.h"

namespace skyline::loader {
    class Loader {
      protected:
        std::string filePath; //!< The path to the ROM file
        std::ifstream file; //!< An input stream from the file

        /**
         * @brief Read the file at a particular offset
         * @tparam T The type of object to write to
         * @param output The object to write to
         * @param offset The offset to read the file at
         * @param size The amount to read in bytes
         */
        template<typename T>
        void ReadOffset(T *output, u32 offset, size_t size) {
            file.seekg(offset, std::ios_base::beg);
            file.read(reinterpret_cast<char *>(output), size);
        }


      public:
        /**
         * @param file_path_ The path to the ROM file
         */
        Loader(std::string &filePath) : filePath(filePath), file(filePath, std::ios::binary | std::ios::beg) {}
    };
}
