// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    // @fmt:off
    /**
    * @brief This enumerates all of the types of an NPad controller
    */
    enum class NpadControllerType : u32 {
        None          = 0, //!< No Controller
        ProController = 0b1, //!< Pro Controller
        Handheld      = 0b10, //!< Handheld Controllers
        JoyconDual    = 0b100, //!< Dual Joy-Cons Controller
        JoyconLeft    = 0b1000, //!< Left Joy-Con Controller
        JoyconRight   = 0b10000, //!< Right Joy-Con Controller
    };
    // @fmt:on

    /**
    * @brief This enumerates all the possible assignments of the Joy-Con(s)
    */
    enum class NpadJoyAssignment : u32 {
        Dual = 0, //!< Dual Joy-Cons (Default)
        Single = 1, //!< Single Joy-Con
    };

    /**
    * @brief This enumerates the status codes for reading NPad colors
    */
    enum class NpadColorReadStatus : u32 {
        Success = 0, //!< The color was read successfully
        Invalid = 1, //!< The color read in wasn't valid
        Disconnected = 2 //!< The controller isn't connected
    };

    /**
    * @brief This structure stores the color of a controller
    */
    struct NpadColor {
        u32 bodyColor; //!< The color of the controller's body
        u32 buttonColor; //!< The color of the controller's buttons
    };
    static_assert(sizeof(NpadColor) == 0x8);

    /**
    * @brief The structure of the NPad headers (https://switchbrew.org/wiki/HID_Shared_Memory#NpadStateHeader)
    */
    struct NpadHeader {
        NpadControllerType type; //!< The type of this controller
        NpadJoyAssignment assignment; //!< The Joy-Con assignment of this controller

        NpadColorReadStatus singleColorStatus; //!< The status of reading color from a single controller (Single Joy-Con or Pro Controller)
        NpadColor singleColor; //!< The color of the single controller

        NpadColorReadStatus dualColorStatus; //!< The status of reading color from dual controllers (Dual Joy-Cons)
        NpadColor rightColor; //!< The color of the right Joy-Con
        NpadColor leftColor; //!< The color of the left Joy-Con
    };
    static_assert(sizeof(NpadHeader) == 0x28);

    /**
     * @brief This is a bit-field of all the buttons on an NPad (https://switchbrew.org/wiki/HID_Shared_Memory#NpadButton)
     */
    union NpadButton {
        u64 raw;
        struct {
            bool a               : 1; //!< The A button
            bool b               : 1; //!< The B button
            bool x               : 1; //!< The X button
            bool y               : 1; //!< The Y button
            bool leftStick       : 1; //!< The Left-Stick button
            bool rightStick      : 1; //!< The Right-Stick button
            bool l               : 1; //!< The L trigger
            bool r               : 1; //!< The R button
            bool zl              : 1; //!< The ZL trigger
            bool zr              : 1; //!< The ZR trigger
            bool plus            : 1; //!< The + button
            bool minus           : 1; //!< The - button
            bool dpadLeft        : 1; //!< D-Pad left
            bool dpadUp          : 1; //!< D-Pad up
            bool dpadRight       : 1; //!< D-Pad right
            bool dpadDown        : 1; //!< D-Pad down
            bool leftStickLeft   : 1; //!< Left stick left
            bool leftStickUp     : 1; //!< Left stick up
            bool leftStickRight  : 1; //!< Left stick right
            bool leftStickDown   : 1; //!< Left stick down
            bool rightStickLeft  : 1; //!< Right stick left
            bool rightStickUp    : 1; //!< Right stick up
            bool rightStickRight : 1; //!< Right stick right
            bool rightStickDown  : 1; //!< Right stick down
            bool leftSl          : 1; //!< Left Joy-Con SL button
            bool leftSr          : 1; //!< Left Joy-Con SR button
            bool rightSl         : 1; //!< Right Joy-Con SL button
            bool rightSr         : 1; //!< Right Joy-Con SR button
        };
    };
    static_assert(sizeof(NpadButton) == 0x8);

    /**
    * @brief This structure holds data about the state of the connection with the controller
    */
    union NpadConnectionState {
        u64 raw;
        struct {
            bool connected : 1; //!< If the controller is connected (Not in handheld mode)
            bool handheld : 1; //!< If the controller is in handheld mode
            bool leftJoyconConnected : 1; //!< If the left Joy-Con is connected (Not in handheld mode)
            bool leftJoyconHandheld : 1; //!< If the left Joy-Con is handheld
            bool rightJoyconConnected : 1; //!< If the right Joy-Con is connected (Not in handheld mode)
            bool rightJoyconHandheld : 1; //!< If the right Joy-Con is handheld
        };
    };
    static_assert(sizeof(NpadConnectionState) == 0x8);

    /**
    * @brief This structure contains data about the controller's current state (https://switchbrew.org/wiki/HID_Shared_Memory#NpadHandheldState)
    */
    struct NpadControllerState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 localTimestamp; //!< The local timestamp in samples

        NpadButton buttons; //!< The state of the buttons

        i32 leftX; //!< The left stick X (32768 to -32768)
        i32 leftY; //!< The left stick Y (32768 to -32768)

        i32 rightX; //!< The right stick X (32768 to -32768)
        i32 rightY; //!< The right stick Y (32768 to -32768)

        NpadConnectionState status; //!< The connection status of the controller
    };
    static_assert(sizeof(NpadControllerState) == 0x30);

    /**
    * @brief This structure contains the header and entries for the controller input
    */
    struct NpadControllerInfo {
        CommonHeader header; //!< The header for this controller
        std::array<NpadControllerState, constant::HidEntryCount> state; //!< An array of all of the entries
    };
    static_assert(sizeof(NpadControllerInfo) == 0x350);

    /**
     * @brief This structure is used to hold a single sample of 3D data from the IMU
     */
    struct SixaxisVector {
        float x; //!< The data in the X-axis
        float y; //!< The data in the Y-axis
        float z; //!< The data in the Z-axis
    };
    static_assert(sizeof(SixaxisVector) == 0xC);

    /**
    * @brief This structure contains data about the state of the controller's IMU (Sixaxis) (https://switchbrew.org/wiki/HID_Shared_Memory#NpadSixAxisSensorHandheldState)
    */
    struct NpadSixaxisState {
        u64 globalTimestamp; //!< The global timestamp in samples
        u64 _unk0_;
        u64 localTimestamp; //!< The local timestamp in samples

        SixaxisVector accelerometer; //!< The data from the accelerometer
        SixaxisVector gyroscope; //!< The data from the gyroscope
        SixaxisVector _unk1_; //!< The data from an unknown sensor
        std::array<SixaxisVector, 3> orientation; //!< The orientation basis data as a matrix

        u64 _unk2_; //!< This is always 1
    };
    static_assert(sizeof(NpadSixaxisState) == 0x68);

    /**
    * @brief This structure contains header and entries for the IMU (Sixaxis) data
    */
    struct NpadSixaxisInfo {
        CommonHeader header; //!< The header for the controller's IMU data
        std::array<NpadSixaxisState, constant::HidEntryCount> state; //!< An array of all of the entries
    };
    static_assert(sizeof(NpadSixaxisInfo) == 0x708);

    /**
    * @brief This is a bit-field of all the device types (https://switchbrew.org/wiki/HID_services#DeviceType)
    */
    union NpadDeviceType {
        u32 raw;
        struct {
            bool fullKey : 1; //!< Pro/GC controller
            bool debugPad : 1; //!< Debug controller
            bool handheldLeft : 1; //!< Left Joy-Con controller in handheld mode
            bool handheldRight : 1; //!< Right Joy-Con controller in handheld mode
            bool joyconLeft : 1; //!< Left Joy-Con controller
            bool joyconRight : 1; //!< Right Joy-Con controller
            bool palma : 1; //!< Poké Ball Plus controller
            bool famicomLeft : 1; //!< Famicom left controller
            bool famicomRight : 1;//!< Famicom right controller
            bool nesLeft : 1; //!< NES left controller
            bool nesRight : 1; //!< NES right controller
            bool handheldFamicomLeft : 1; //!< Famicom left controller in handheld mode
            bool handheldFamicomRight : 1;//!< Famicom right controller in handheld mode
            bool handheldNesLeft : 1; //!< NES left controller in handheld mode
            bool handheldNesRight : 1; //!< NES right controller in handheld mode
            bool lucia : 1; //!< SNES controller
            u32 _unk_ : 15;
            bool system : 1; //!< Generic controller
        };
    };
    static_assert(sizeof(NpadDeviceType) == 0x4);

    /**
    * @brief This structure holds the system properties of this NPad (https://switchbrew.org/wiki/HID_Shared_Memory#NpadSystemProperties)
    */
    union NpadSystemProperties {
        u64 raw;
        struct {
            bool singleCharging : 1; //!< If a single unit is charging (Handheld, Pro-Con)
            bool leftCharging : 1; //!< If the left Joy-Con is charging
            bool rightCharging : 1; //!< If the right Joy-Con is charging
            bool singlePowerConnected : 1; //!< If a single unit is connected to a power source (Handheld, Pro-Con)
            bool leftPowerConnected : 1; //!< If the left Joy-Con is connected to a power source
            bool rightPowerConnected : 1; //!< If the right Joy-Con is connected to a power source
            u64 _unk_ : 3;
            bool unsupportedButtonPressedSystem : 1; //!< If an unsupported buttons was pressed on system controller
            bool unsupportedButtonPressedSystemExt : 1; //!< If an unsupported buttons was pressed on system external controller
            bool abxyButtonsOriented : 1; //!< If the ABXY Buttons oriented
            bool slSrButtonOriented : 1; //!< If the SL/SR Buttons oriented
            bool plusButtonCapability : 1; //!< If the + button exists
            bool minusButtonCapability : 1; //!< If the - button exists
            bool directionalButtonsSupported : 1; //!< If the controller has a D-Pad
        };
    };
    static_assert(sizeof(NpadSystemProperties) == 0x8);

    /**
    * @brief This structure holds data about the System Buttons (Home, Sleep and Capture) on an NPad (https://switchbrew.org/wiki/HID_Shared_Memory#NpadSystemButtonProperties)
    */
    union NpadSystemButtonProperties {
        u32 raw;
        struct {
            bool unintendedHomeButtonInputProtectionEnabled : 1; //!< If the Unintended Home Button Input Protection is enabled or not
        };
    };
    static_assert(sizeof(NpadSystemButtonProperties) == 0x4);

    /**
     * @brief This enumerates all the possible values for the NPad's battery level
     */
    enum class NpadBatteryLevel : u32 {
        Empty = 0, //!< The battery is empty
        Low = 1, //!< The battery is nearly empty
        Medium = 2, //!< The battery is fairly full
        High = 3, //!< The battery is almost full
        Full = 4, //!< The battery is 100% full
    };

    /**
    * @brief The structure of the Npad section (https://switchbrew.org/wiki/HID_Shared_Memory#NpadState)
    */
    struct NpadSection {
        NpadHeader header; //!< The header for this section

        NpadControllerInfo fullKeyController; //!< The Pro/GC controller data
        NpadControllerInfo handheldController; //!< The Handheld controller data
        NpadControllerInfo dualController; //!< The Dual Joy-Con controller data (Only in Dual Mode, no input rotation based on rotation)
        NpadControllerInfo leftController; //!< The Left Joy-Con controller data (Only in Single Mode, no input rotation based on rotation)
        NpadControllerInfo rightController; //!< The Right Joy-Con controller data (Only in Single Mode, no input rotation based on rotation)
        NpadControllerInfo digitalController; //!< The Default Digital controller data (Same as Default but Analog Sticks are converted into 8-directional Digital Sticks)
        NpadControllerInfo defaultController; //!< The Default controller data (Inputs are rotated based on orientation and SL/SR are mapped to L/R incase it is a single JC)

        NpadSixaxisInfo fullKeySixaxis; //!< The Pro/GC IMU data
        NpadSixaxisInfo handheldSixaxis; //!< The Handheld IMU data
        NpadSixaxisInfo dualLeftSixaxis; //!< The Left Joy-Con in dual mode IMU data
        NpadSixaxisInfo dualRightSixaxis; //!< The Left Joy-Con in dual mode IMU data
        NpadSixaxisInfo leftSixaxis; //!< The Left Joy-Con IMU data
        NpadSixaxisInfo rightSixaxis; //!< The Right Joy-Con IMU data

        NpadDeviceType deviceType; //!< The device type of this controller

        u32 _pad0_;

        NpadSystemProperties systemProperties; //!< The system properties of this controller
        NpadSystemButtonProperties buttonProperties; //!< The system button properties of this controller
        NpadBatteryLevel singleBatteryLevel; //!< The battery level of a single unit (Handheld, Pro-Con)
        NpadBatteryLevel leftBatteryLevel; //!< The battery level of the left Joy-Con
        NpadBatteryLevel rightBatteryLevel; //!< The battery level of the right Joy-Con

        u32 _pad1_[0x395];
    };
    static_assert(sizeof(NpadSection) == 0x5000);
}
