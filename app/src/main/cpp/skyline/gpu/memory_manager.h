// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    namespace constant {
        constexpr u64 GpuPageSize{1 << 16}; //!< The page size of the GPU address space
    }

    namespace gpu::vmm {
        enum class ChunkState {
            Unmapped, //!< The chunk is unmapped
            Reserved, //!< The chunk is reserved
            Mapped //!< The chunk is mapped and a CPU side address is present
        };

        struct ChunkDescriptor {
            u64 virtAddr; //!< The address of the chunk in the virtual address space
            u64 size; //!< The size of the chunk in bytes
            u8 *cpuPtr; //!< A pointer to the chunk in the application's address space (if mapped)
            ChunkState state;

            ChunkDescriptor(u64 virtAddr, u64 size, u8 *cpuPtr, ChunkState state) : virtAddr(virtAddr), size(size), cpuPtr(cpuPtr), state(state) {}

            /**
             * @return If the given chunk can be contained wholly within this chunk
             */
            inline bool CanContain(const ChunkDescriptor &chunk) {
                return (chunk.virtAddr >= this->virtAddr) && ((this->size + this->virtAddr) >= (chunk.size + chunk.virtAddr));
            }
        };

        /**
         * @brief The MemoryManager class handles mapping between a virtual address space and an application's address space
         */
        class MemoryManager {
          private:
            const DeviceState &state;
            std::vector<ChunkDescriptor> chunks;
            std::shared_mutex vmmMutex;

            /**
             * @brief Finds a chunk in the virtual address space that is larger than meets the given requirements
             * @note vmmMutex MUST be locked when calling this
             * @param desiredState The state of the chunk to find
             * @param size The minimum size of the chunk to find
             * @param alignment The minimum alignment of the chunk to find
             * @return The first applicable chunk
             */
            std::optional<ChunkDescriptor> FindChunk(ChunkState desiredState, u64 size, u64 alignment = 0);

            /**
             * @brief Inserts a chunk into the chunk list, resizing and splitting as necessary
             * @note vmmMutex MUST be locked when calling this
             * @param newChunk The chunk to insert
             * @return The base virtual address of the inserted chunk
             */
            u64 InsertChunk(const ChunkDescriptor &newChunk);

          public:
            MemoryManager(const DeviceState &state);

            /**
             * @brief Reserves a region of the virtual address space so it will not be chosen automatically when mapping
             * @param size The size of the region to reserve
             * @param alignment The alignment of the region to reserve
             * @return The base virtual address of the reserved region
             */
            u64 ReserveSpace(u64 size, u64 alignment);

            /**
             * @brief Reserves a fixed region of the virtual address space so it will not be chosen automatically when mapping
             * @param virtAddr The virtual base address of the region to allocate
             * @param size The size of the region to allocate
             * @return The base virtual address of the reserved region
             */
            u64 ReserveFixed(u64 virtAddr, u64 size);

            /**
             * @brief Maps a CPU memory region into an automatically chosen region of the virtual address space
             * @param cpuPtr A pointer to the region to be mapped into the virtual address space
             * @param size The size of the region to map
             * @return The base virtual address of the mapped region
             */
            u64 MapAllocate(u8 *cpuPtr, u64 size);

            /**
             * @brief Maps a CPU memory region to a fixed region in the virtual address space
             * @param virtAddr The target virtual address of the region
             * @param cpuPtr A pointer to the region to be mapped into the virtual address space
             * @param size The size of the region to map
             * @return The base virtual address of the mapped region
             */
            u64 MapFixed(u64 virtAddr, u8 *cpuPtr, u64 size);

            /**
             * @brief Unmaps all chunks in the given region from the virtual address space
             * @return Whether the operation succeeded
             */
            bool Unmap(u64 virtAddr, u64 size);

            void Read(u8 *destination, u64 virtAddr, u64 size);

            /**
             * @brief Reads in a span from a region of the virtual address space
             */
            template<typename T>
            void Read(span <T> destination, u64 virtAddr) {
                Read(reinterpret_cast<u8 *>(destination.data()), virtAddr, destination.size_bytes());
            }

            /**
             * @brief Reads in an object from a region of the virtual address space
             * @tparam T The type of object to return
             */
            template<typename T>
            T Read(u64 virtAddr) {
                T obj;
                Read(reinterpret_cast<u8 *>(&obj), virtAddr, sizeof(T));
                return obj;
            }

            void Write(u8 *source, u64 virtAddr, u64 size);

            /**
             * @brief Writes out a span to a region of the virtual address space
             */
            template<typename T>
            void Write(span <T> source, u64 virtAddr) {
                Write(reinterpret_cast<u8 *>(source.data()), virtAddr, source.size_bytes());
            }

            /**
             * @brief Reads in an object from a region of the virtual address space
             */
            template<typename T>
            void Write(T source, u64 virtAddr) {
                Write(reinterpret_cast<u8 *>(&source), virtAddr, sizeof(T));
            }
        };
    }
}
