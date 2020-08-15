// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "npad_device.h"

namespace skyline::input {
    struct GuestController {
        NpadControllerType type{}; //!< The type of the controller
        i8 partnerIndex{-1}; //!< The index of a Joy-Con partner, if this has one
        NpadDevice *device{nullptr}; //!< A pointer to the NpadDevice that all events from this are redirected to
    };

    /**
     * @brief This class is used to manage all NPad devices and their allocations to Player objects
     */
    class NpadManager {
      private:
        const DeviceState &state; //!< The state of the device
        std::array<NpadDevice, constant::NpadCount> npads; //!< An array of all the NPad devices
        bool activated{false}; //!< If this NpadManager is activated or not
        std::atomic<bool> updated{false}; //!< If this NpadManager has been updated by the guest

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
        std::array<GuestController, constant::ControllerCount> controllers; //!< An array of all the available guest controllers
        std::vector<NpadId> supportedIds; //!< The NpadId(s) that are supported by the application
        NpadStyleSet styles; //!< The styles that are supported by the application
        NpadJoyOrientation orientation{}; //!< The Joy-Con orientation to use

        /**
         * @param hid A pointer to HID Shared Memory on the host
         */
        NpadManager(const DeviceState &state, input::HidSharedMemory *hid);

        /**
         * @param id The ID of the NPad to return
         * @return A reference to the NPad with the specified ID
         */
        constexpr inline NpadDevice &at(NpadId id) {
            return npads.at(Translate(id));
        }

        /**
         * @param id The ID of the NPad to return
         * @return A reference to the NPad with the specified ID
         */
        constexpr inline NpadDevice &operator[](NpadId id) noexcept {
            return npads.operator[](Translate(id));
        }

        /**
        * @brief This deduces all the mappings from guest controllers -> players based on the configuration supplied by HID services and available controllers
         * @param host If the update is host-initiated rather than the guest
        */
        void Update(bool host = false);

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
