// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/asset_manager.h>
#include "android_asset_backing.h"
#include "android_asset_filesystem.h"

namespace skyline::vfs {
    AndroidAssetFileSystem::AndroidAssetFileSystem(AAssetManager *backing) : FileSystem(), backing(backing) {}

    std::shared_ptr<Backing> AndroidAssetFileSystem::OpenFileImpl(const std::string &path, Backing::Mode mode) {
        auto file{AAssetManager_open(backing, path.c_str(), AASSET_MODE_RANDOM)};
        if (file == nullptr)
            return nullptr;

        return std::make_shared<AndroidAssetBacking>(file, mode);
    }

    std::optional<Directory::EntryType> AndroidAssetFileSystem::GetEntryTypeImpl(const std::string &path) {
        // Tries to open as a file then as a directory in order to check the type
        // This is really hacky but it at least it works
        if (AAssetManager_open(backing, path.c_str(), AASSET_MODE_RANDOM) != nullptr)
            return Directory::EntryType::File;

        if (AAssetManager_openDir(backing, path.c_str()) != nullptr)
            return Directory::EntryType::Directory;

        // Doesn't exit
        return std::nullopt;
    }

    std::shared_ptr<Directory> AndroidAssetFileSystem::OpenDirectoryImpl(const std::string &path, Directory::ListMode listMode) {
        throw exception("AndroidAssetFileSystem directories are unimplemented");
    }
}
