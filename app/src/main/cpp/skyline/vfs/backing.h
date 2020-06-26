// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::vfs {
    /**
     * @brief The Backing class represents an abstract region of memory
     */
    class Backing {
      public:
        /**
         * @brief This describes the modes that a backing is capable of
         */
        union Mode {
            struct {
                bool read : 1; //!< The backing is readable
                bool write : 1; //!< The backing is writable
                bool append : 1; //!< The backing can be appended
            };
            u32 raw; //!< The raw value of the mode
        };
        static_assert(sizeof(Mode) == 0x4);

        Mode mode; //!< The mode of the backing
        size_t size; //!< The size of the backing in bytes

        /**
         * @param mode The mode to use for the backing
         * @param size The initial size of the backing
         */
        Backing(Mode mode = {true, false, false}, size_t size = 0) : mode(mode), size(size) {}

        /* Delete the move constructor to prevent multiple instances of the same backing */
        Backing(const Backing &) = delete;

        Backing &operator=(const Backing &) = delete;

        virtual ~Backing() = default;

        /**
         * @brief Read bytes from the backing at a particular offset to a buffer
         * @param output The object to write to
         * @param offset The offset to start reading from
         * @param size The amount to read in bytes
         * @return The amount of bytes read
         */
        virtual size_t Read(u8 *output, size_t offset, size_t size) = 0;

        /**
         * @brief Read bytes from the backing at a particular offset to a buffer (template version)
         * @tparam T The type of object to write to
         * @param output The object to write to
         * @param offset The offset to start reading from
         * @param size The amount to read in bytes
         * @return The amount of bytes read
         */
        template<typename T>
        inline size_t Read(T *output, size_t offset = 0, size_t size = 0) {
            return Read(reinterpret_cast<u8 *>(output), offset, size ? size : sizeof(T));
        }
    };
}
