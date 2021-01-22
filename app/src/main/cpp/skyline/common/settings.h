// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    /**
     * @brief The Settings class is used to access preferences set in the Kotlin component of Skyline
     */
    class Settings {
      public:
        int logLevel; //!< The minimum level that logs need to be for them to be printed
        bool operationMode; //!< If the emulated Switch should be handheld or docked
        std::string username; //!< The name set by the user to be supplied to the guest

        /**
         * @param fd An FD to the preference XML file
         */
        Settings(int fd);
    };
}
