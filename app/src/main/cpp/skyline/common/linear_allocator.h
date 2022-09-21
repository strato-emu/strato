// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    /**
     * @brief Typeless allocation state holder for LinearAllocator<T>
     * @tparam NewChunkSize Step size in bytes for overflow chunk allocations
     */
    template<size_t NewChunkSize = (1024 * 1024)> // Default to 1MB
    class LinearAllocatorState {
      private:
        std::vector<u8> mainChunk; //!< The primary backing for the allocator, will grow during `Reset` calls if the previous set of allocations overflowed
        std::list<std::vector<u8>> overflowChunks; //!< Overflow backing chunks used to hold allocations that can't fit in  `mainChunk` until it can be resized
        u8 *ptr{}; //!< Points to a free region of memory of size `chunkRemainingBytes`
        size_t chunkRemainingBytes{NewChunkSize}; //!< Remaining bytes left in the current chunk

        size_t allocCount{}; //!< The number of currently unfreed allocations

      public:
        LinearAllocatorState() {
            mainChunk.reserve(NewChunkSize);
            ptr = mainChunk.data();
        }

        /**
         * @brief Allocates a contiguous chunk of memory of size `size` aligned to the maximum possible required alignment
         */
        u8 *Allocate(size_t unalignedSize, bool track = true) {
            // Align the size to the maximum required alignment for standard types
            size_t size{util::AlignUp(unalignedSize, alignof(std::max_align_t))};

            // Allocated memory cannot span across multiple chunks
            if (size > NewChunkSize)
                throw std::bad_alloc();

            if (chunkRemainingBytes < size) {
                // If there is no space left in the current chunk allocate a new overflow one
                auto &newChunk{overflowChunks.emplace_back()};
                newChunk.reserve(NewChunkSize);
                ptr = newChunk.data();
                chunkRemainingBytes = NewChunkSize;
            }

            auto allocatedPtr{ptr};

            // Move ourselves along
            chunkRemainingBytes -= size;
            ptr += size;

            if (track)
                allocCount++;

            return allocatedPtr;
        }

        template<typename T>
        T *AllocateUntracked() {
            return reinterpret_cast<T *>(Allocate(sizeof(T), false));
        }

        template<typename T>
        span<T> AllocateUntracked(size_t count) {
            return {reinterpret_cast<T *>(Allocate(sizeof(T) * count, false)), count};
        }

        template<typename T, class... Args>
        T *EmplaceUntracked(Args &&... args) {
            return std::construct_at(AllocateUntracked<T>(), std::forward<Args>(args)...);
        }

        /**
         * @brief Decreases allocation count, should be called an equal number of times to `Allocate`
         */
        void Deallocate() {
            allocCount--;
        }

        /**
         * @brief Resizes the main chunk to fit any previously needed overflow chunks and allows memory to be reused again for further allocations
         * @note There **must** be no allocations leftover when this is called
         */
        void Reset() {
            if (allocCount)
                // If we still have allocations remaining then throw
                throw std::bad_alloc();

            if (size_t overflowSize{overflowChunks.size() * NewChunkSize}) {
                // Expand the main chunk so that it can fit any previously needed overflow chunks
                overflowChunks.clear();
                mainChunk.reserve(overflowSize + mainChunk.capacity());
            }

            ptr = mainChunk.data();
            chunkRemainingBytes = mainChunk.capacity();
        }
    };

    /**
     * @brief Linear allocator conforming to the C++ 'Allocator' named requirement, forwards all allocation requests to the passed in state
     */
    template<typename T, typename State = LinearAllocatorState<>>
    class LinearAllocator {
      private:
        State &state; //!< Backing allocation state holder

      public:
        using value_type = T;

        template<typename, typename> friend class LinearAllocator; //!< Required for copy ctor from other types

        /**
         * @note This is explicitly not explicit to avoid the need to repeat the allocator type when constructing a new object
         */
        LinearAllocator(State &state) : state(state) {}

        template<typename U>
        LinearAllocator(const LinearAllocator<U> &other) : state(other.state) {};

        template<typename U>
        LinearAllocator(LinearAllocator<U> &&other) : state(other.state) {};

        [[nodiscard]] T *allocate(size_t n) {
            return reinterpret_cast<T *>(state.Allocate(sizeof(T) * n));
        }

        void deallocate(T *obj, size_t n) noexcept {
            state.Deallocate();
        }

        bool operator==(const LinearAllocator &other) const noexcept {
            return &state == &other.state;
        }

        bool operator!=(const LinearAllocator &other) const noexcept {
            return &state != &other.state;
        }
    };
}
