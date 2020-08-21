// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "npad_device.h"

namespace skyline::input {
    /**
     * @brief A controller equivalent to a physical one connected to the Switch, it's translation into a Player (NpadDevice) is also encapsulated here
     */
    struct GuestController {
        NpadControllerType type{};
        i8 partnerIndex{-1}; //!< The index of a Joy-Con partner, if this has one
        NpadDevice *device{nullptr}; //!< A pointer to the NpadDevice that all events from this are redirected to
    };

    /**
     * @brief This class is used to manage all NPad devices and their allocations to Player objects
     */
    class NpadManager {
      private:
        const DeviceState &state;
        bool activated{false}; //!< If this NpadManager is activated or not

        friend NpadDevice;

        /**
         * @brief This translates an NPad's ID into it's index in the array
         * @param id The ID of the NPad to translate
         * @return The corresponding index of the NPad in the array
         */
        static constexpr inline size_t Translate(NpadId id) {
            switch (id) {
                case NpadId::Handheld:
                    return 8;
                case NpadId::Unknown:
                    return 9;
                default:
                    return static_cast<size_t>(id);
            }
        }

      public:
        std::recursive_mutex mutex; //!< This mutex must be locked before any modifications to class members
        std::array<NpadDevice, constant::NpadCount> npads;
        std::array<GuestController, constant::ControllerCount> controllers;
        std::vector<NpadId> supportedIds; //!< The NpadId(s) that are supported by the application
        NpadStyleSet styles; //!< The styles that are supported by the application
        NpadJoyOrientation orientation{}; //!< The orientation all of Joy-Cons are in (This affects stick transformation for them)

        /**
         * @param hid A pointer to HID Shared Memory on the host
         */
        NpadManager(const DeviceState &state, input::HidSharedMemory *hid);

        /**
         * @return A reference to the NPad with the specified ID
         */
        constexpr inline NpadDevice &at(NpadId id) {
            return npads.at(Translate(id));
        }

        /**
         * @return A reference to the NPad with the specified ID
         */
        constexpr inline NpadDevice &operator[](NpadId id) noexcept {
            return npads.operator[](Translate(id));
        }

        /**
         * @brief This deduces all the mappings from guest controllers -> players based on the configuration supplied by HID services and available controllers
         * @note If any class members were edited, the mutex shouldn't be released till this is called
         */
        void Update();

        /**
         * @brief This activates the mapping between guest controllers -> players, a call to this is required for function
         */
        void Activate();

        /**
         * @brief This disables any activate mappings from guest controllers -> players till Activate has been called
         */
        void Deactivate();
    };
}
