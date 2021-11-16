// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <queue>
#include <common.h>
#include <common/address_space.h>
#include <services/common/result.h>

namespace skyline::service::nvdrv::core {
    /**
     * @brief The nvmap core class holds the global state for nvmap and provides methods to manage handles
     */
    class NvMap {
      public:
        /**
         * @brief A handle to a contiguous block of memory in an application's address space
         */
        struct Handle {
            std::mutex mutex;

            u64 align{}; //!< The alignment to use when pinning the handle onto the SMMU
            u64 size; //!< Page-aligned size of the memory the handle refers to
            u64 alignedSize;  //!< `align`-aligned size of the memory the handle refers to
            u64 origSize; //!< Original unaligned size of the memory this handle refers to

            i32 dupes{1}; //!< How many guest references there are to this handle
            i32 internalDupes{0}; //!< How many emulator-internal references there are to this handle

            using Id = u32;
            Id id; //!< A globally unique identifier for this handle

            i32 pins{};
            u32 pinVirtAddress{};
            std::optional<std::list<std::shared_ptr<Handle>>::iterator> unmapQueueEntry{};

            struct Flags {
                bool mapUncached : 1; //!< If the handle should be mapped as uncached
                bool _pad0_ : 1;
                bool keepUncachedAfterFree : 1; //!< Only applicable when the handle was allocated with a fixed address
                bool _pad1_ : 1;
                bool _unk0_ : 1; //!< Passed to IOVMM for pins
                u32 _pad2_ : 27;
            } flags{};
            static_assert(sizeof(Flags) == sizeof(u32));

            u64 address{}; //!< The memory location in the guest's AS that this handle corresponds to, this can also be in the nvdrv tmem
            bool isSharedMemMapped{}; //!< If this nvmap has been mapped with the MapSharedMem IPC call

            u8 kind{}; //!< Used for memory compression
            bool allocated{}; //!< If the handle has been allocated with `Alloc`

            Handle(u64 size, Id id);

            /**
             * @brief Sets up the handle with the given memory config, can allocate memory from the tmem if a 0 address is passed
             */
            [[nodiscard]] PosixResult Alloc(Flags pFlags, u32 pAlign, u8 pKind, u64 pAddress);

            /**
             * @brief Increases the dupe counter of the handle for the given session
             */
            [[nodiscard]] PosixResult Duplicate(bool internalSession);

            /**
             * @brief Obtains a pointer to the handle's memory and marks the handle it as having been mapped
             */
            u8 *GetPointer() {
                if (!address)
                    throw exception("Cannot get a pointer to the memory of an unallocated handle!");

                isSharedMemMapped = true;
                return reinterpret_cast<u8 *>(address);
            }
        };

      private:
        const DeviceState &state;

        FlatAllocator<u32, 0, 32> smmuAllocator;
        std::list<std::shared_ptr<Handle>> unmapQueue;
        std::mutex unmapQueueLock; //!< Protects access to `unmapQueue`

        std::unordered_map<Handle::Id, std::shared_ptr<Handle>> handles; //!< Main owning map of handles
        std::mutex handlesLock; //!< Protects access to `handles`

        static constexpr u32 HandleIdIncrement{4}; //!< Each new handle ID is an increment of 4 from the previous
        std::atomic<u32> nextHandleId{HandleIdIncrement};

        void AddHandle(std::shared_ptr<Handle> handle);

        /**
         * @brief Unmaps and frees the SMMU memory region a handle is mapped to
         * @note Both `unmapQueueLock` and `handleDesc.mutex` MUST be locked when calling this
         */
        void UnmapHandle(Handle &handleDesc);

        /**
         * @brief Removes a handle from the map taking its dupes into account
         * @note handleDesc.mutex MUST be locked when calling this
         * @return If the handle was removed from the map
         */
        bool TryRemoveHandle(const Handle &handleDesc);

      public:
        /**
         * @brief Encapsulates the result of a FreeHandle operation
         */
        struct FreeInfo {
            u64 address; //!< Address the handle referred to before deletion
            u64 size; //!< Page-aligned handle size
            bool wasUncached; //!< If the handle was allocated as uncached
        };

        NvMap(const DeviceState &state);

        /**
         * @brief Creates an unallocated handle of the given size
         */
        [[nodiscard]] PosixResultValue<std::shared_ptr<Handle>> CreateHandle(u64 size);

        std::shared_ptr<Handle> GetHandle(Handle::Id handle);

        /**
         * @brief Maps a handle into the SMMU address space
         * @note This operation is refcounted, the number of calls to this must eventually match the number of calls to `UnpinHandle`
         * @return The SMMU virtual address that the handle has been mapped to
         */
        u32 PinHandle(Handle::Id handle);

        /**
         * @brief When this has been called an equal number of times to `PinHandle` for the supplied handle it will be added to a list of handles to be freed when necessary
         */
        void UnpinHandle(Handle::Id handle);

        /**
         * @brief Tries to free a handle and remove a single dupe
         * @note If a handle has no dupes left and has no other users a FreeInfo struct will be returned describing the prior state of the handle
         */
        std::optional<FreeInfo> FreeHandle(Handle::Id handle, bool internalSession);
    };
}
