// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <android/asset_manager.h>
#include "android_asset_backing.h"

namespace skyline::vfs {
    AndroidAssetBacking::AndroidAssetBacking(AAsset *asset, Mode mode) : Backing(mode), asset(asset) {
        if (mode.write || mode.append)
            throw exception("AndroidAssetBacking doesn't support writing");

        size = AAsset_getLength64(asset);
    }

    AndroidAssetBacking::~AndroidAssetBacking() {
        AAsset_close(asset);
    }

    size_t AndroidAssetBacking::ReadImpl(span<u8> output, size_t offset) {
        if (AAsset_seek64(asset, offset, SEEK_SET) != offset)
            throw exception("Failed to seek asset position");

        auto result{AAsset_read(asset, output.data(), output.size())};
        if (result < 0)
            throw exception("Failed to read from fd: {}", strerror(errno));

        return static_cast<size_t>(result);
    }
}
