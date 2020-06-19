// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "backing.h"

namespace skyline::vfs {
    /**
     * @brief The OsBacking class provides the backing abstractions for a physical linux file
     */
    class OsBacking : public Backing {
      private:
        int fd; //!< An FD to the backing

      public:
        /**
         * @param fd The file descriptor of the backing
         */
        OsBacking(int fd);

        size_t Read(u8 *output, size_t offset, size_t size);
    };
}