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

      protected:
        size_t ReadImpl(span <u8> output, size_t offset) override {
            return backing->Read(output, baseOffset + offset);
        }

      public:
        /**
         * @param file The backing to create the RegionBacking from
         * @param offset The offset of the region start within the parent backing
         * @param size The size of the region in the parent backing
         */
        RegionBacking(const std::shared_ptr<vfs::Backing> &backing, size_t offset, size_t size, Mode mode = {true, false, false}) : Backing(mode, size), backing(backing), baseOffset(offset) {
            if (mode.write || mode.append)
                throw exception("Cannot open a RegionBacking as writable");
        };
    };
}
