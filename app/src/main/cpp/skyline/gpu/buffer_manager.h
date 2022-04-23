// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "buffer.h"

namespace skyline::gpu {
    /**
     * @brief A simple linearly allocated GPU-side buffer used to temporarily store buffer modifications allowing them to be replayed in-sequence on the GPU
     */
    class MegaBuffer {
      private:
        constexpr static vk::DeviceSize Size{0x6'400'000}; //!< Size in bytes of the megabuffer (100MiB)

        memory::Buffer backing; //!< The backing GPU buffer
        std::mutex mutex; //!< Synchronizes access to freeRegion
        span<u8> freeRegion; //!< Span of unallocated space in the megabuffer

      public:
        MegaBuffer(GPU &gpu);

        /**
         * @brief Resets the free region of the megabuffer to its initial state, data is left intact but may be overwritten
         */
        void Reset();

        /**
         * @brief Returns the underlying Vulkan buffer for the megabuffer
         */
        vk::Buffer GetBacking() const;

        /**
         * @brief Pushes data to the megabuffer and returns the offset at which it was written
         * @param pageAlign Whether the pushed data should be page aligned in the megabuffer
         */
        vk::DeviceSize Push(span<u8> data, bool pageAlign = false);
    };

    /**
     * @brief The Buffer Manager is responsible for maintaining a global view of buffers being mapped from the guest to the host, any lookups and creation of host buffer from equivalent guest buffer alongside reconciliation of any overlaps with existing textures
     */
    class BufferManager {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the buffer mappings
        std::vector<std::shared_ptr<Buffer>> buffers; //!< A sorted vector of all buffer mappings

        /**
         * @return If the end of the supplied buffer is less than the supplied pointer
         */
        static bool BufferLessThan(const std::shared_ptr<Buffer> &it, u8 *pointer);

      public:
        MegaBuffer megaBuffer; //!< The megabuffer used to temporarily store buffer modifications allowing them to be replayed in-sequence on the GPU

        BufferManager(GPU &gpu);

        /**
         * @return A pre-existing or newly created Buffer object which covers the supplied mappings
         */
        BufferView FindOrCreate(GuestBuffer guestMapping, const std::shared_ptr<FenceCycle> &cycle = nullptr);
    };
}
