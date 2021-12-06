// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "buffer.h"

namespace skyline::gpu {
    /**
     * @brief The Buffer Manager is responsible for maintaining a global view of buffers being mapped from the guest to the host, any lookups and creation of host buffer from equivalent guest buffer alongside reconciliation of any overlaps with existing textures
     */
    class BufferManager {
      private:
        /**
         * @brief A single contiguous mapping of a buffer in the CPU address space
         */
        struct BufferMapping : span<u8> {
            std::shared_ptr<Buffer> buffer;
            GuestBuffer::Mappings::iterator iterator; //!< An iterator to the mapping in the buffer's GuestBufferMappings corresponding to this mapping
            vk::DeviceSize offset; //!< Offset of this mapping relative to the start of the buffer

            template<typename... Args>
            BufferMapping(std::shared_ptr<Buffer> buffer, GuestBuffer::Mappings::iterator iterator, vk::DeviceSize offset, Args &&... args)
                : span<u8>(std::forward<Args>(args)...),
                  buffer(std::move(buffer)),
                  iterator(iterator),
                  offset(offset) {}
        };

        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the buffer mappings
        std::vector<BufferMapping> buffers; //!< A sorted vector of all buffer mappings

      public:
        BufferManager(GPU &gpu);

        /**
         * @return A pre-existing or newly created Buffer object which covers the supplied mappings
         */
        std::shared_ptr<BufferView> FindOrCreate(const GuestBuffer &guest);
    };
}
