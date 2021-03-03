// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "backing.h"

struct AAsset;

namespace skyline::vfs {
    /**
     * @brief The AndroidAssetBacking class provides the backing abstractions for the AAsset Android API
     * @note This is NOT thread safe NOR should it be shared across threads.
     * @note This will take ownership of the backing asset passed into it
     */
    class AndroidAssetBacking : public Backing {
      private:
        AAsset *backing; //!< The backing AAsset object we abstract

      protected:
        size_t ReadImpl(span<u8> output, size_t offset) override;

      public:
        AndroidAssetBacking(AAsset *backing, Mode mode = {true, false, false});

        virtual ~AndroidAssetBacking();
    };
}
