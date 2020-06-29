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
        /**
         * @brief This enumerates the types of entry a directory can contain
         */
        enum class EntryType : u32 {
            Directory = 0x0, //!< The entry is a directory
            File = 0x1, //!< The entry is a file
        };

        /**
         * @brief This hold information about a single directory entry
         */
        struct Entry {
            std::string name; //!< The name of the entry
            EntryType type; //!< The type of the entry
        };

        /**
         * @brief This describes what will be returned when reading a directories contents
         */
        union ListMode {
            struct {
                bool directory : 1; //!< The directory listing will contain subdirectories
                bool file : 1; //!< The directory listing will contain files
            };
            u32 raw; //!< The raw value of the listing mode
        };
        static_assert(sizeof(ListMode) == 0x4);

        ListMode listMode; //!< This describes what this directory will return when it is Read

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
