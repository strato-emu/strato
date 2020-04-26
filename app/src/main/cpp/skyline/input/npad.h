// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "shared_mem.h"

namespace skyline {
    namespace constant {
        constexpr u32 NpadBatteryFull = 2; //!< The full battery state of an npad
        constexpr u8 NpadCount = 10; //!< The number of npads in shared memory
    }

    namespace input::npad {
        union NpadButton {
            struct {
                bool a : 1; //!< The A button
                bool b : 1; //!< The B button
                bool x : 1; //!< The X button
                bool y : 1; //!< The Y button
                bool l3 : 1; //!< The L3 button
                bool r3 : 1; //!< The R3 button
                bool l : 1; //!< The L trigger
                bool r : 1; //!< The R button
                bool zl : 1; //!< The ZL trigger
                bool zr : 1; //!< The ZR trigger
                bool plus : 1; //!< The + button
                bool minus : 1; //!< The - button
                bool dpadLeft : 1; //!< D-Pad left
                bool dpadUp : 1; //!< D-Pad up
                bool dpadRight : 1; //!< D-Pad right
                bool dpadDown : 1; //!< D-Pad down
                bool leftStickLeft : 1; //!< Left stick left
                bool leftStickUp : 1; //!< Left stick up
                bool leftStickRight : 1; //!< Left stick right
                bool leftStickDown : 1; //!< Left stick down
                bool rightStickLeft : 1; //!< Right stick left
                bool rightStickUp : 1; //!< Right stick up
                bool rightStickRight : 1; //!< Right stick right
                bool rightStickDown : 1; //!< Right stick down
                bool leftSL : 1; //!< Left Joy-Con SL button
                bool leftSr : 1; //!< Left Joy-Con SR button
                bool rightSl : 1; //!< Right Joy-Con SL button
                bool rightSr : 1; //!< Right Joy-Con SR button
            };
            u64 raw;
        };
        static_assert(sizeof(NpadButton) == 0x8);

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
        * @brief This holds the controller styles supported
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
        * @brief This holds a Controller's ID (https://switchbrew.org/wiki/HID_services#NpadIdType)
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
        * @brief This denotes the orientation of the Joy-Con(s)
        */
        enum class NpadJoyOrientation : u64 {
            Vertical = 0, //!< The Joy-Con is held vertically
            Horizontal = 1, //!< The Joy-Con is held horizontally
            Unset = 2 //!< Not set
        };

        /**
        * @brief This denotes the assignment of the Joy-Con(s)
        */
        enum class NpadJoyAssignment : u32 {
            Dual = 0, //!< Dual Joy-Cons
            Single = 1, //!< Single Joy-Con
            Unset = 2 //!< Not set
        };

        /**
        * @brief This denotes the colour read status of an npad
        */
        enum class NpadColourReadStatus : u32 {
            Success = 0, //!< Success
            Invalid = 1, //!< Invalid color
            Disconnected = 2 //!< Controller not connected
        };

        /**
        * @brief This denotes the type of an npad
        */
        enum class NpadControllerType {
            None, //!< Nothing
            ProController, //!< A Pro Controller
            Handheld, //!< Handheld mode
            JoyconDual, //!< Dual Joy-Cons
            JoyconLeft, //!< Left Joy-Con
            JoyconRight, //!< Right Joy-Con
            Palma, //!< Poké Ball Plus
        };

        /**
        * @brief This denotes the connection state of an npad
        */
        union NpadConnectionState {
            struct {
                bool connected : 1; //!< Is connected
                bool handheld : 1; //!< Is in handheld mode
                bool leftJoyconConnected : 1; //!< Is the left Joy-Con connected
                bool leftJoyconHandheld : 1; //!< Is the left Joy-Con handheld
                bool rightJoyconConnected : 1; //!< Is the right Joy-Con connected
                bool rightJoyconHandheld : 1; //!< Is the right Joy-Con handheld
            };
            u64 raw;
        };
        static_assert(sizeof(NpadConnectionState) == 0x8);

        /**
        * @brief This denotes the device type of an npad
        */
        union NpadDeviceType {
            struct {
                bool fullKey : 1; //!< Pro/GC controller
                bool handheld : 1; //!< Handheld mode
                bool handheldLeft : 1; //!< Joy-Con/Famicom/NES left controller
                bool handheldRight : 1; //!< Joy-Con/Famicom/NES right controller
                bool joyconLeft : 1; //!< Left Joy-Con
                bool joyconRight : 1; //!< Right Joy-Con
                bool palma : 1; //!< Pokeball Plus controller
                bool larkLeftFamicom : 1; //!< Famicom left Joy-Con
                bool larkRightFamicom : 1;//!< Famicom right Joy-Con
                bool larkLeftNES : 1; //!< NES left Joy-Con
                bool larkRightNES : 1; //!< NES right Joy-Con
                u32 _unk1_ : 4;
                bool systemExt : 1; //!< Generic external controller
                u32 _unk2_ : 14;
                bool system : 1; //!< Generic controller
            };
            u32 raw;
        };
        static_assert(sizeof(NpadDeviceType) == 0x4);

        /**
        * @brief This denotes the system properties of the npad
        */
        union NpadSystemProperties {
            struct {
                bool powerInfo0Charging : 1; //!< Info 0 Charging
                bool powerInfo1Charging : 1; //!< Info 1 Charging
                bool powerInfo2Charging : 1; //!< Info 2 Charging
                bool powerInfo0PowerConnected : 1; //!< Info 0 Connected
                bool powerInfo1PowerConnected : 1; //!< Info 1 Connected
                bool powerInfo2PowerConnected : 1; //!< Info 2 Connected
                u64 _unk_ : 3;
                bool unsupportedButtonPressedSystem : 1; //!< Unsupported buttons are pressed on system controller
                bool unsupportedButtonPressedSystemExt : 1; //!< Unsupported buttons are pressed on system external controller
                bool ABXYButtonOriented : 1; //!< Are the ABXY Buttons oriented
                bool SLSRuttonOriented : 1; //!< Are the SLSR Buttons oriented
                bool plusButtonCapability : 1; //!< Does the + button exist
                bool minusButtonCapability : 1; //!< Does the - button exist
                bool directionalButtonsSupported : 1; //!< Does the controller have a D-Pad
            };
            u64 raw;
        };
        static_assert(sizeof(NpadSystemProperties) == 0x8);

        /**
        * @brief This denotes the system button properties of the npad
        */
        union NpadSystemButtonProperties {
            struct {
                bool unintendedHomeButtonInputProtectionEnabled : 1; //!< Is unintended home button input protection enabled
            };
            u32 raw;
        };
        static_assert(sizeof(NpadSystemButtonProperties) == 0x4);

        /**
        * @brief This denotes the colour of an npad
        */
        struct NpadColour {
            u32 bodyColour; //!< The body colour
            u32 buttonColour; //!< The button colour
        };
        static_assert(sizeof(NpadColour) == 0x8);

        /**
        * @brief This is the header of an npad entry
        */
        struct NpadHeader {
            NpadStyleSet styles; //!< The style set
            NpadJoyAssignment assignment; //!< The pad assignment

            NpadColourReadStatus singleColourStatus; //!< The single colour status
            NpadColour singleColour; //!< The colours

            NpadColourReadStatus dualColourStatus; //!< The dual colour status
            NpadColour rightColour; //!< The right colours
            NpadColour leftColour; //!< The left colours
        };
        static_assert(sizeof(NpadHeader) == 0x28);

        /**
        * @brief This contains controller input data
        */
        struct NpadController {
            NpadButton buttons; //!< The pressed buttons

            u32 leftX; //!< The left stick X
            u32 leftY; //!< The left stick Y

            u32 rightX; //!< The right stick X
            u32 rightY; //!< The right stick Y
        };
        static_assert(sizeof(NpadController) == 0x18);

        /**
        * @brief This contains info about controller input data
        */
        struct NpadControllerState {
            u64 globalTimestamp; //!< The global timestamp
            u64 localTimestamp; //!< The local timestamp
            NpadController controller; //!< The npad controller
            NpadConnectionState status; //!< The npad connection status
        };
        static_assert(sizeof(NpadControllerState) == 0x30);

        /**
        * @brief This contains all the input states
        */
        struct NpadControllerInfo {
            CommonHeader header; //!< The common data header
            std::array<NpadControllerState, 17> state; //!< The npad state
        };
        static_assert(sizeof(NpadControllerInfo) == 0x350);

        /**
        * @brief An npad section in shared memory
        */
        struct NpadSection {
            NpadHeader header; //!< The npad header

            NpadControllerInfo fullKeyController; //!< The full key controller
            NpadControllerInfo handheldController; //!< The handheld controller
            NpadControllerInfo dualController; //!< The dual Joy-Con controller
            NpadControllerInfo leftController; //!< The left Joy-Con controller
            NpadControllerInfo rightController; //!< The right Joy-Con controller
            NpadControllerInfo pokeballController; //!< The Pokeball Plus controller
            NpadControllerInfo systemExtController; //!< The system external controller

            u64 _unk_[0xE1 * 6]; //!< Unused sixaxis data

            NpadDeviceType deviceType; //!< The device type

            u32 _pad0_;

            NpadSystemProperties properties; //!< The system properties
            NpadSystemButtonProperties buttonProperties; //!< The button properties

            u32 batteryLevel[3]; //!< The battery level reported

            u32 _pad1_[0x395];
        };
        static_assert(sizeof(NpadSection) == 0x5000);

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
}
