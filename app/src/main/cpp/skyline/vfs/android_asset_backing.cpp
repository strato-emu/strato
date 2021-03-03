// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <android/asset_manager.h>
#include "android_asset_backing.h"

namespace skyline::vfs {
    AndroidAssetBacking::AndroidAssetBacking(AAsset *backing, Mode mode) : Backing(mode), backing(backing) {
        if (mode.write || mode.append)
            throw exception("AndroidAssetBacking doesn't support writing");

        size = AAsset_getLength64(backing);
    }

    AndroidAssetBacking::~AndroidAssetBacking() {
        AAsset_close(backing);
    }

    size_t AndroidAssetBacking::ReadImpl(span<u8> output, size_t offset) {
        if (AAsset_seek64(backing, offset, SEEK_SET) != offset)
            throw exception("Failed to seek asset position");

        auto ret{AAsset_read(backing, output.data(), output.size())};
        if (ret < 0)
            throw exception("Failed to read from fd: {}", strerror(errno));

        return static_cast<size_t>(ret);
    }
}
