// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u64 GpuPageSize{1 << 16}; //!< The page size of the GPU address space
    }

    namespace gpu::vmm {
        enum ChunkState {
            Unmapped, //!< The chunk is unmapped
            Reserved, //!< The chunk is reserved
            Mapped //!< The chunk is mapped and a CPU side address is present
        };

        struct ChunkDescriptor {
            u64 address; //!< The address of the chunk in the GPU address space
            u64 size; //!< The size of the chunk in bytes
            u8* pointer; //!< A pointer to the chunk in the CPU address space (if mapped)
            ChunkState state;

            ChunkDescriptor(u64 address, u64 size, u8* pointer, ChunkState state) : address(address), size(size), pointer(pointer), state(state) {}

            /**
             * @return If the given chunk can be contained wholly within this chunk
             */
            inline bool CanContain(const ChunkDescriptor &chunk) {
                return (chunk.address >= this->address) && ((this->size + this->address) >= (chunk.size + chunk.address));
            }
        };

        /**
         * @brief The MemoryManager class handles the mapping of the GPU address space
         */
        class MemoryManager {
          private:
            const DeviceState &state;
            std::vector<ChunkDescriptor> chunks;

            /**
             * @brief Finds a chunk of the specified type in the GPU address space that is larger than the given size
             * @param state The state of the chunk to find
             * @param size The minimum size of the chunk to find
             * @param alignment The alignment of the chunk to find
             * @return The first unmapped chunk in the GPU address space that fulfils the requested conditions
             */
            std::optional<ChunkDescriptor> FindChunk(ChunkState state, u64 size, u64 alignment = 0);

            /**
             * @brief Inserts a chunk into the chunk list, resizing and splitting as necessary
             * @param newChunk The chunk to insert
             * @return The base virtual GPU address of the inserted chunk
             */
            u64 InsertChunk(const ChunkDescriptor &newChunk);

          public:
            MemoryManager(const DeviceState &state);

            /**
             * @brief Reserves a region of the GPU address space so it will not be chosen automatically when mapping
             * @param size The size of the region to reserve
             * @param alignment The alignment of the region to reserve
             * @return The virtual GPU base address of the region base
             */
            u64 ReserveSpace(u64 size, u64 alignment);

            /**
             * @brief Reserves a fixed region of the GPU address space so it will not be chosen automatically when mapping
             * @param address The virtual base address of the region to allocate
             * @param size The size of the region to allocate
             * @return The virtual address of the region base
             */
            u64 ReserveFixed(u64 address, u64 size);

            /**
             * @brief Maps a physical CPU memory region to an automatically chosen virtual memory region
             * @param pointer A pointer to the region to be mapped into the GPU's address space
             * @param size The size of the region to map
             * @return The virtual address of the region base
             */
            u64 MapAllocate(u8* pointer, u64 size);

            /**
             * @brief Maps a physical CPU memory region to a fixed virtual memory region
             * @param address The target virtual address of the region
             * @param pointer A pointer to the region to be mapped into the GPU's address space
             * @param size The size of the region to map
             * @return The virtual address of the region base
             */
            u64 MapFixed(u64 address, u8* pointer, u64 size);

            /**
             * @brief Unmaps all chunks in the given region from the GPU address space
             * @return Whether the operation succeeded
             */
            bool Unmap(u64 address, u64 size);

            void Read(u8 *destination, u64 address, u64 size) const;

            /**
             * @brief Reads in a span from a region of the GPU virtual address space
             */
            template<typename T>
            void Read(span<T> destination, u64 address) const {
                Read(reinterpret_cast<u8 *>(destination.data()), address, destination.size_bytes());
            }

            /**
             * @brief Reads in an object from a region of the GPU virtual address space
             * @tparam T The type of object to return
             */
            template<typename T>
            T Read(u64 address) const {
                T obj;
                Read(reinterpret_cast<u8 *>(&obj), address, sizeof(T));
                return obj;
            }

            void Write(u8 *source, u64 address, u64 size) const;

            /**
             * @brief Writes out a span to a region of the GPU virtual address space
             */
            template<typename T>
            void Write(span<T> source, u64 address) const {
                Write(reinterpret_cast<u8 *>(source.data()), address, source.size_bytes());
            }

            /**
             * @brief Reads in an object from a region of the GPU virtual address space
             */
            template<typename T>
            void Write(T source, u64 address) const {
                Write(reinterpret_cast<u8 *>(&source), address, sizeof(T));
            }
        };
    }
}
