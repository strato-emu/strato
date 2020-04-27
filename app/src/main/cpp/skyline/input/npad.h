// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "shared_mem.h"

namespace skyline::input::npad {
    enum class NpadAxisId {
        RX, //!< Right stick X
        RY, //!< Right stick Y
        LX, //!< Left stick X
        LY //!< Left stick Y
    };

    enum class NpadButtonState : bool {
        Released = false, //!< Released button
        Pressed = true //!< Pressed button
    };

    /**
    * @brief This enumerates all the possible IDs for an NPad (https://switchbrew.org/wiki/HID_services#NpadIdType)
    */
    enum class NpadId : u32 {
        Player1 = 0x0, //!< 1st Player
        Player2 = 0x1, //!< 2nd Player
        Player3 = 0x2, //!< 3rd Player
        Player4 = 0x3, //!< 4th Player
        Player5 = 0x4, //!< 5th Player
        Player6 = 0x5, //!< 6th Player
        Player7 = 0x6, //!< 7th Player
        Player8 = 0x7, //!< 8th Player
        Unknown = 0x10, //!< Unknown
        Handheld = 0x20 //!< Handheld mode
    };

    /**
    * @brief This enumerates all the orientations of the Joy-Con(s)
    */
    enum class NpadJoyOrientation : u64 {
        Vertical = 0, //!< The Joy-Con is held vertically
        Horizontal = 1, //!< The Joy-Con is held horizontally
    };
    /**
    * @brief This holds all the NPad styles (https://switchbrew.org/wiki/HID_services#NpadStyleTag)
    */
    union NpadStyleSet {
        struct {
            bool proController : 1; //!< The Pro Controller
            bool joyconHandheld : 1; //!< Joy-Cons in handheld mode
            bool joyconDual : 1; //!< Joy-Cons in a pair
            bool joyconLeft : 1; //!< Left Joy-Con only
            bool joyconRight : 1; //!< Right Joy-Con only
            bool gamecube : 1; //!< GameCube controller
            bool palma : 1;  //!< Poké Ball Plus controller
            bool nes : 1; //!< NES controller
            bool nesHandheld : 1; //!< NES controller in handheld mode
            bool snes : 1; //!< SNES controller
        };
        u32 raw;
    };
    static_assert(sizeof(NpadStyleSet) == 0x4);

    /**
    * @brief Converts the ID of an npad to the index in shared memory
    * @param id The ID of an npad
    * @return The index in shared memory
    */
    u32 NpadIdToIndex(NpadId id);

    /**
    * @brief Converts the index in shared memory to the ID of an npad
    * @param id The index in shared memory
    * @return The ID of the npad
    */
    NpadId IndexToNpadId(u32 index);

    class NpadDevice {
      private:
        NpadId id; //!< The ID of the npad
        NpadControllerType controllerType{NpadControllerType::None}; //!< The type of controller
        uint stateEntryIndex{}; //!< The index of the current state entry

        NpadConnectionState connectionState{}; //!< The state of the connection
        NpadSection &shmemSection; //!< The section in HID shared memory for this controller

        /**
        * @brief Updates headers for a new shared memory entry
        * @param controller The controller to operate on
        * @param lastStateEntryIndex The index of the previous state entry
        */
        void UpdateHeaders(NpadControllerInfo &controller, uint lastStateEntryIndex);

        /**
        * @return The controller device info appropriate for the controllers type
        */
        NpadControllerInfo &GetControllerDeviceInfo();

      public:
        bool supported{false}; //!< If the npad marked as supported

        /**
        * @param shmemSection A reference to the controllers shared memory region
        * @param id The ID of the npad
        */
        NpadDevice(NpadSection &shmemSection, NpadId id);

        /**
        * @brief Sets the joycon assignment in shared memory
        * @param assignment The assignment to set
        */
        inline void SetAssignment(NpadJoyAssignment assignment) {
            shmemSection.header.assignment = assignment;
        }

        /**
        * @brief Connects a controller to the guest
        * @param type The type of controller to connect
        */
        void Connect(NpadControllerType type);

        /**
        * @brief Disconnects the controller from the guest
        */
        void Disconnect();

        /**
        * @brief Changes a button's state on the virtual controller
        * @param button The button work on
        * @param state Whether to release or press the button
        */
        void SetButtonState(NpadButton button, NpadButtonState state);

        /**
        * @brief Sets an axis to a value on the virtual controller
        * @param axis The axis to change
        * @param value The value to use
        */
        void SetAxisValue(NpadAxisId axis, int value);
    };

    class CommonNpad {
      private:
        const DeviceState &state; //!< The state of the device

      public:
        NpadStyleSet supportedStyles{}; //!< The supported style sets
        NpadJoyOrientation orientation{NpadJoyOrientation::Unset}; //!< The Joy-Con orientation to use

        CommonNpad(const DeviceState &state);

        /**
        * @brief Activates npad support
        */
        void Activate();
    };
}
