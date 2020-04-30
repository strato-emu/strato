// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "npad_device.h"

namespace skyline::input {
    /**
     * @brief This class is used to
     */
    class NpadManager {
      private:
        const DeviceState &state; //!< The state of the device
        std::array<NpadDevice, constant::NpadCount> npads; //!< An array of all the NPad devices

        /**
         * @brief This translates an NPad's ID into it's index in the array
         * @param id The ID of the NPad to translate
         * @return The corresponding index of the NPad in the array
         */
        constexpr inline size_t Translate(NpadId id) {
            switch (id) {
                case NpadId::Unknown:
                    return 8;
                case NpadId::Handheld:
                    return 9;
                default:
                    return static_cast<size_t>(id);
            }
        }

      public:
        NpadStyleSet styles{}; //!< The styles that are supported in accordance to the host input
        NpadJoyOrientation orientation{}; //!< The Joy-Con orientation to use

        /**
         * @param hid A pointer to HID Shared Memory on the host
         */
        NpadManager(const DeviceState &state, input::HidSharedMemory *hid);

        /**
         * @param id The ID of the NPad to return
         * @return A reference to the NPad with the specified ID
         */
        constexpr inline NpadDevice& at(NpadId id) {
            return npads.at(Translate(id));
        }

        /**
         * @param id The ID of the NPad to return
         * @return A reference to the NPad with the specified ID
         */
        constexpr inline NpadDevice& operator[](NpadId id) noexcept {
            return npads.operator[](Translate(id));
        }

        /**
        * @brief This activates the controllers
        */
        void Activate();

        /**
         * @brief This deactivates the controllers
         */
        void Deactivate();
    };
}
