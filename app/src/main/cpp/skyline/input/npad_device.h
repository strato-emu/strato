// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "shared_mem.h"

namespace skyline::input {
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
            bool proController  : 1; //!< The Pro Controller
            bool joyconHandheld : 1; //!< Joy-Cons in handheld mode
            bool joyconDual     : 1; //!< Joy-Cons in a pair
            bool joyconLeft     : 1; //!< Left Joy-Con only
            bool joyconRight    : 1; //!< Right Joy-Con only
            bool gamecube       : 1; //!< GameCube controller
            bool palma          : 1; //!< Poké Ball Plus controller
            bool nes            : 1; //!< NES controller
            bool nesHandheld    : 1; //!< NES controller in handheld mode
            bool snes           : 1; //!< SNES controller
        };
        u32 raw;
    };
    static_assert(sizeof(NpadStyleSet) == 0x4);

    /**
     * @brief This enumerates the states that a button can be in
     */
    enum class NpadButtonState : bool {
        Released = false, //!< The button isn't being pressed
        Pressed = true //!< The button is being pressed
    };

    /**
     * @brief This enumerates all of the axis on NPad controllers
     */
    enum class NpadAxisId {
        RX, //!< Right Stick X
        RY, //!< Right Stick Y
        LX, //!< Left Stick X
        LY //!< Left Stick Y
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

    class NpadDevice {
      private:
        NpadId id; //!< The ID of this controller
        NpadControllerType controllerType{}; //!< The type of this controller
        u8 globalTimestamp{}; //!< The global timestamp of the state entries

        NpadConnectionState connectionState{}; //!< The state of the connection
        NpadSection &section; //!< The section in HID shared memory for this controller

        /**
        * @brief This updates the headers and creates a new entry in HID Shared Memory
        * @param info The controller info of the NPad that needs to be updated
        * @return The next entry that has been created with values from the last entry
        */
        NpadControllerState &GetNextEntry(NpadControllerInfo &info);

        /**
        * @return The NpadControllerInfo for this controller based on it's type
        */
        NpadControllerInfo &GetControllerInfo();

      public:
        bool supported{false}; //!< If this specific NpadId was marked by the application as supported

        NpadDevice(NpadSection &section, NpadId id);

        /**
        * @brief This sets a Joy-Con's Assignment Mode
        * @param assignment The assignment mode to set this controller to
        */
        inline void SetAssignment(NpadJoyAssignment assignment) {
            section.header.assignment = assignment;
        }

        /**
        * @brief This connects this controller to the guest
        * @param type The type of controller to connect
        */
        void Connect(NpadControllerType type);

        /**
        * @brief This disconnects this controller from the guest
        */
        void Disconnect();

        /**
        * @brief This changes the state of buttons to the specified state
        * @param mask A bit-field mask of all the buttons to change
        * @param state The state to change the buttons to
        */
        void SetButtonState(NpadButton mask, NpadButtonState state);

        /**
        * @brief This sets the value of an axis to the specified value
        * @param axis The axis to set the value of
        * @param value The value to set
        */
        void SetAxisValue(NpadAxisId axis, i32 value);
    };
}
