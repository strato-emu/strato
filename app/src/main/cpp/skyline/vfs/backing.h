// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::vfs {
    /**
     * @brief The Backing class provides abstract access to a storage device, all access can be done without using a specific backing
     */
    class Backing {
      public:
        /**
         * @brief The capabilities of the Backing
         */
        union Mode {
            struct {
                bool read : 1; //!< The backing is readable
                bool write : 1; //!< The backing is writable
                bool append : 1; //!< The backing can be appended
            };
            u32 raw;
        } mode;
        static_assert(sizeof(Mode) == 0x4);

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
         * @param output The object to write to
         * @param offset The offset to start reading from
         * @param size The amount to read in bytes
         * @return The amount of bytes read
         */
        template<typename T>
        inline size_t Read(T *output, size_t offset = 0, size_t size = 0) {
            return Read(reinterpret_cast<u8 *>(output), offset, size ? size : sizeof(T));
        }

        /**
         * @brief Writes from a buffer to a particular offset in the backing
         * @param input The object to write to the backing
         * @param offset The offset where the input buffer should be written
         * @param size The amount to write
         * @return The amount of bytes written
         */
        virtual size_t Write(u8 *input, size_t offset, size_t size) {
            throw exception("This backing does not support being written to");
        }

        /**
         * @brief Writes from a buffer to a particular offset in the backing (template version)
         * @tparam T The type of object to write
         * @param input The object to write to the backing
         * @param offset The offset where the input buffer should be written
         * @param size The amount to write
         * @return The amount of bytes written
         */
        template<typename T>
        inline size_t Write(T *output, size_t offset = 0, size_t size = 0) {
            return Write(reinterpret_cast<u8 *>(output), offset, size ? size : sizeof(T));
        }

        /**
         * @brief Resizes a backing to the given size
         * @param size The new size for the backing
         */
        virtual void Resize(size_t size) {
            throw exception("This backing does not support being resized");
        }
    };
}
