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
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes access to the buffer mappings
        std::vector<std::shared_ptr<Buffer>> buffers; //!< A sorted vector of all buffer mappings

        /**
         * @return If the end of the supplied buffer is less than the supplied pointer
         */
        static bool BufferLessThan(const std::shared_ptr<Buffer> &it, u8 *pointer);

      public:
        BufferManager(GPU &gpu);

        /**
         * @return A pre-existing or newly created Buffer object which covers the supplied mappings
         */
        BufferView FindOrCreate(GuestBuffer guestMapping, const std::shared_ptr<FenceCycle> &cycle = nullptr);
    };
}
