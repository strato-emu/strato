// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "language.h"

namespace skyline {
    /**
     * @brief The Settings class provides a simple interface to access user-defined settings, update values and subscribe callbacks to observe changes.
     */
    class Settings {
        template<typename T>
        class Setting {
            using Callback = std::function<void(const T &)>;
            std::vector<Callback> callbacks; //!< Callbacks to be called when this setting changes
            T value;
            std::mutex valueMutex;
            std::mutex callbackMutex;

            /**
             * @brief Calls all callbacks registered for this setting
             * @note Locking of the setting value must be handled by the caller
             */
            void OnSettingChanged() {
                std::scoped_lock lock{callbackMutex};
                for (const auto &callback : callbacks)
                    callback(value);
            }

          public:
            /**
             * @return The underlying setting value
             */
            const T &operator*() {
                std::scoped_lock lock{valueMutex};
                return value;
            }

            /**
             * @brief Sets the underlying setting value, signalling any callbacks if necessary
             */
            void operator=(T newValue) {
                std::scoped_lock lock{valueMutex};
                if (value != newValue) {
                    value = std::move(newValue);
                    OnSettingChanged();
                }
            }

            /**
             * @brief Register a callback to be run when this setting changes
             */
            void AddCallback(Callback callback) {
                std::scoped_lock lock{callbackMutex};
                callbacks.push_back(std::move(callback));
            }
        };

      public:
        // System
        Setting<bool> isDocked; //!< If the emulated Switch should be handheld or docked
        Setting<std::string> usernameValue; //!< The user name to be supplied to the guest
        Setting<language::SystemLanguage> systemLanguage; //!< The system language
        Setting<region::RegionCode> systemRegion; //!< The system region

        // Display
        Setting<bool> forceTripleBuffering; //!< If the presentation engine should always triple buffer even if the swapchain supports double buffering
        Setting<bool> disableFrameThrottling; //!< Allow the guest to submit frames without any blocking calls

        // GPU
        Setting<std::string> gpuDriver; //!< The label of the GPU driver to use
        Setting<std::string> gpuDriverLibraryName; //!< The name of the GPU driver library to use
        Setting<u32> executorSlotCountScale; //!< Number of GPU executor slots that can be used concurrently

        // Hacks
        Setting<bool> enableFastGpuReadbackHack; //!< If the CPU texture readback skipping hack should be used

        // Audio
        Setting<bool> isAudioOutputDisabled; //!< Disables audio output

        // Debug
        Setting<bool> validationLayer; //!< If the vulkan validation layer is enabled

        Settings() = default;

        virtual ~Settings() = default;

        /**
         * @brief Updates settings with the given values
         * @note This method is platform-specific and must be overridden
         */
        virtual void Update() = 0;
    };
}
