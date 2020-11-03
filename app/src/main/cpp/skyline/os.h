// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "loader/loader.h"
#include "services/serviceman.h"

namespace skyline::kernel {
    /**
     * @brief The OS class manages the interaction between the various Skyline components
     */
    class OS {
      public:
        DeviceState state;
        service::ServiceManager serviceManager;
        std::string appFilesPath; //!< The full path to the app's files directory

        /**
         * @param logger An instance of the Logger class
         * @param settings An instance of the Settings class
         * @param window The ANativeWindow object to draw the screen to
         */
        OS(std::shared_ptr<JvmManager> &jvmManager, std::shared_ptr<Logger> &logger, std::shared_ptr<Settings> &settings, const std::string &appFilesPath);

        /**
         * @brief Execute a particular ROM file
         * @param romFd A FD to the ROM file to execute
         * @param romType The type of the ROM file
         */
        void Execute(int romFd, loader::RomFormat romType);

        /**
         * @brief Creates a new process
         * @param entry The entry point for the new process
         * @param argument The argument for the initial function
         * @param stackSize The size of the main stack
         * @return An instance of the KProcess of the created process
         */
        std::shared_ptr<type::KProcess> CreateProcess(u64 entry, u64 argument, size_t stackSize);
    };
}
