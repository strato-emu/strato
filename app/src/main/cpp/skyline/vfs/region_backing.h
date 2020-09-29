// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "backing.h"

namespace skyline::vfs {
    /**
     * @brief The RegionBacking class provides a way to create a new, smaller backing from a region of an existing backing
     */
    class RegionBacking : public Backing {
      private:
        std::shared_ptr<vfs::Backing> backing; //!< The parent backing
        size_t baseOffset; //!< The offset of the region in the parent backing

      public:
        /**
         * @param file The backing to create the RegionBacking from
         * @param offset The offset of the region start within the parent backing
         * @param size The size of the region in the parent backing
         */
        RegionBacking(const std::shared_ptr<vfs::Backing> &backing, size_t offset, size_t size, Mode mode = {true, false, false}) : Backing(mode, size), backing(backing), baseOffset(offset) {};

        virtual size_t Read(span<u8> output, size_t offset = 0) {
            if (!mode.read)
                throw exception("Attempting to read a backing that is not readable");
            if (size - offset < output.size())
                throw exception("Trying to read past the end of a region backing: 0x{:X}/0x{:X} (Offset: 0x{:X})", output.size(), size, offset);
            return backing->Read(output, baseOffset + offset);
        }
    };
}
