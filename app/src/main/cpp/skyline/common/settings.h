// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "language.h"

namespace skyline {
    /**
     * @brief The Settings class is used to access preferences set in the Kotlin component of Skyline
     */
    class Settings {
      public:
        // System
        bool operationMode; //!< If the emulated Switch should be handheld or docked
        std::string usernameValue; //!< The name set by the user to be supplied to the guest
        language::SystemLanguage systemLanguage; //!< The system language set by the user

        // Display
        bool forceTripleBuffering; //!< If the presentation engine should always triple buffer even if the swapchain supports double buffering
        bool disableFrameThrottling; //!< Allow the guest to submit frames without any blocking calls

        template<class T>
        Settings(T settings) {
            Update(settings);
        }

        /**
         * @brief Updates settings with the given values
         * @param newSettings A platform-specific object containing the new settings' values
         */
        template<class T>
        void Update(T newSettings);
    };
}
