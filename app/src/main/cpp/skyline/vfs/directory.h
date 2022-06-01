// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::vfs {
    /**
     * @brief The Directory class represents an abstract directory in a filesystem
     */
    class Directory {
      public:
        enum class EntryType : u8 {
            Directory = 0x0,
            File = 0x1,
        };

        struct Entry {
            std::string name;
            EntryType type;
            size_t size; //!< 0 if a directory
        };

        /**
         * @brief A descriptor for what will be returned when reading a directories contents
         */
        union ListMode {
            struct {
                bool directory : 1; //!< The directory listing will contain subdirectories
                bool file : 1; //!< The directory listing will contain files
            };
            u32 raw{};
        };
        static_assert(sizeof(ListMode) == 0x4);

        ListMode listMode;

        Directory(ListMode listMode) : listMode(listMode) {}

        /* Delete the move constructor to prevent multiple instances of the same directory */
        Directory(const Directory &) = delete;

        Directory &operator=(const Directory &) = delete;

        virtual ~Directory() = default;

        /**
         * @brief Reads the contents of a directory non-recursively
         * @return A vector containing each entry
         */
        virtual std::vector<Entry> Read() = 0;
    };
}
