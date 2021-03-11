// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KSharedMemory is used to retain two mappings of the same underlying memory, allowing sharing memory between two processes
     */
    class KSharedMemory : public KMemory {
      private:
        int fd; //!< A file descriptor to the underlying shared memory
        memory::MemoryState memoryState; //!< The state of the memory as supplied initially, this is retained for any mappings

      public:
        struct MapInfo {
            u8 *ptr;
            size_t size;

            constexpr bool Valid() {
                return ptr && size;
            }
        } kernel, guest{};

        KSharedMemory(const DeviceState &state, size_t size, memory::MemoryState memState = memory::states::SharedMemory, KType type = KType::KSharedMemory);

        /**
         * @note 'ptr' needs to be in guest-reserved address space
         */
        u8 *Map(u8 *ptr, u64 size, memory::Permission permission);

        span<u8> Get() override {
            return span(guest.ptr, guest.size);
        }

        void UpdatePermission(u8 *ptr, size_t size, memory::Permission permission) override;

        /**
         * @brief The destructor of shared memory, it deallocates the memory from all processes
         */
        ~KSharedMemory();
    };
}
