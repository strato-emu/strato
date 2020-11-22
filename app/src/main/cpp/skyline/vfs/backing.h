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
         * @param output The object to write the data read to
         * @param offset The offset to start reading from
         * @return The amount of bytes read
         */
        virtual size_t Read(span <u8> output, size_t offset = 0) = 0;

        /**
         * @brief Implicit casting for reading into spans of different types
         */
        template<class T, typename std::enable_if<!std::is_same_v<T, u8>, bool>::type = true>
        inline size_t Read(span <T> output, size_t offset = 0) {
            return Read(output.template cast<u8>(), offset);
        }

        /**
         * @brief Read bytes from the backing at a particular offset into an object
         * @param offset The offset to start reading from
         * @return The object that was read
         */
        template<typename T>
        inline T Read(size_t offset = 0) {
            T object;
            Read(span(reinterpret_cast<u8 *>(&object), sizeof(T)), offset);
            return object;
        }

        /**
         * @brief Writes from a buffer to a particular offset in the backing
         * @param input The data to write to the backing
         * @param offset The offset where the input buffer should be written
         * @return The amount of bytes written
         */
        virtual size_t Write(span <u8> input, size_t offset = 0) {
            throw exception("This backing does not support being written to");
        }

        /**
         * @brief Writes from an object into a particular offset in the backing
         * @param object The object to write to the backing
         * @param offset The offset where the input should be written
         */
        template<typename T>
        inline void WriteObject(const T &object, size_t offset = 0) {
            size_t size;
            if ((size = Write(span(reinterpret_cast<u8 *>(&object), sizeof(T)), offset)) != sizeof(T))
                throw exception("Object wasn't written fully into output backing: {}/{}", size, sizeof(T));
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
