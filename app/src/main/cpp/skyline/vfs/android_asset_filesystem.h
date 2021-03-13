// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "filesystem.h"

struct AAssetManager;

namespace skyline::vfs {
    /**
     * @brief  The AndroidAssetFileSystem class provides the filesystem backing abstractions for the AAsset Android API
     */
    class AndroidAssetFileSystem : public FileSystem {
      private:
        AAssetManager *assetManager; //!< The NDK asset manager for the filesystem

      protected:
        std::shared_ptr<Backing> OpenFileImpl(const std::string &path, Backing::Mode mode) override;

        std::optional<Directory::EntryType> GetEntryTypeImpl(const std::string &path) override;

        std::shared_ptr<Directory> OpenDirectoryImpl(const std::string &path, Directory::ListMode listMode) override;

      public:
        AndroidAssetFileSystem(AAssetManager *assetManager);
    };
}
