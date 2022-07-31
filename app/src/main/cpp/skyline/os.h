// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/language.h>
#include "vfs/filesystem.h"
#include "loader/loader.h"
#include "services/serviceman.h"

namespace skyline::kernel {
    /**
     * @brief The OS class manages the interaction between the various Skyline components
     */
    class OS {
      public:
        std::string nativeLibraryPath; //!< The full path to the app's native library directory
        std::string publicAppFilesPath; //!< The full path to the app's public files directory
        std::string privateAppFilesPath; //!< The full path to the app's private files directory
        std::string deviceTimeZone; //!< The timezone name (e.g. Europe/London)
        std::shared_ptr<vfs::FileSystem> assetFileSystem; //!< A filesystem to be used for accessing emulator assets (like tzdata)
        DeviceState state;
        service::ServiceManager serviceManager;

        /**
         * @param settings An instance of the Settings class
         * @param window The ANativeWindow object to draw the screen to
         */
        OS(
            std::shared_ptr<JvmManager> &jvmManager,
            std::shared_ptr<Settings> &settings,
            std::string publicAppFilesPath,
            std::string privateAppFilesPath,
            std::string deviceTimeZone,
            std::string nativeLibraryPath,
            std::shared_ptr<vfs::FileSystem> assetFileSystem
        );

        /**
         * @brief Execute a particular ROM file
         * @param romFd A FD to the ROM file to execute
         * @param romType The type of the ROM file
         */
        void Execute(int romFd, loader::RomFormat romType);
    };
}
