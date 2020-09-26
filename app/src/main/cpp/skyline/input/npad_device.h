// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include "shared_mem.h"

namespace skyline::constant {
    constexpr jlong MsInSecond{1000}; //!< The amount of milliseconds in a single second of time
    constexpr jint AmplitudeMax{std::numeric_limits<u8>::max()}; //!< The maximum amplitude for Android Vibration APIs
    constexpr i8 NullIndex{-1}; //!< The placeholder index value when there is no device present
}

namespace skyline::input {
    /**
     * @brief This enumerates the orientations the Joy-Con(s) can be held in
     */
    enum class NpadJoyOrientation : i64 {
        Vertical = 0,
        Horizontal = 1,
    };

    /**
     * @brief A union of all the NPad styles (https://switchbrew.org/wiki/HID_services#NpadStyleTag)
     */
    union NpadStyleSet {
        u32 raw;
        struct {
            bool proController  : 1; //!< Pro Controller
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
    };
    static_assert(sizeof(NpadStyleSet) == 0x4);

    /**
     * @brief This enumerates all of the axis on NPads
     */
    enum class NpadAxisId {
        LX, //!< Left Stick X
        LY, //!< Left Stick Y
        RX, //!< Right Stick X
        RY, //!< Right Stick Y
    };

    /**
     * @brief This enumerates all the possible IDs for an NPad (https://switchbrew.org/wiki/HID_services#NpadIdType)
     */
    enum class NpadId : u32 {
        Player1 = 0x0,
        Player2 = 0x1,
        Player3 = 0x2,
        Player4 = 0x3,
        Player5 = 0x4,
        Player6 = 0x5,
        Player7 = 0x6,
        Player8 = 0x7,
        Unknown = 0x10,
        Handheld = 0x20,
    };

    /**
     * @brief A handle to a specific device addressed by it's ID and type
     * @note This is used by both Six-Axis and Vibration
     */
    union __attribute__((__packed__)) NpadDeviceHandle {
        u32 raw;
        struct {
            u8 type;
            NpadId id               : 8;
            bool isRight            : 1; //!< If this is a right Joy-Con (Both) or right LRA in the Pro-Controller (Vibration)
            bool isSixAxisSingle    : 1; //!< If the Six-Axis device is a single unit, either Handheld or Pro-Controller
        };

        constexpr NpadControllerType GetType() const {
            switch (type) {
                case 3:
                    return NpadControllerType::ProController;
                case 4:
                    return NpadControllerType::Handheld;
                case 5:
                    return NpadControllerType::JoyconDual;
                case 6:
                    return NpadControllerType::JoyconLeft;
                case 7:
                    return NpadControllerType::JoyconRight;
            }
            return NpadControllerType::None;
        }
    };

    /**
     * @brief The parameters to produce a vibration using an LRA
     * @note The vibration is broken into a frequency band with the lower and high range supplied
     * @note Amplitude is in arbitrary units from 0f to 1f
     * @note Frequency is in Hertz
     */
    struct NpadVibrationValue {
        float amplitudeLow;
        float frequencyLow;
        float amplitudeHigh;
        float frequencyHigh;
    };
    static_assert(sizeof(NpadVibrationValue) == 0x10);

    class NpadManager;

    /**
     * @brief This class abstracts a single NPad device that controls it's own state and shared memory section
     */
    class NpadDevice {
      private:
        NpadManager &manager; //!< The manager responsible for managing this NpadDevice
        NpadSection &section; //!< The section in HID shared memory for this controller
        NpadControllerInfo *controllerInfo; //!< The NpadControllerInfo for this controller's type
        u64 globalTimestamp{}; //!< An incrementing timestamp that's common across all sections

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
        NpadId id;
        i8 index{constant::NullIndex}; //!< The index of the device assigned to this player
        i8 partnerIndex{constant::NullIndex}; //!< The index of a partner device, if present
        NpadVibrationValue vibrationLeft; //!< Vibration for the left Joy-Con (Handheld/Pair), left LRA in a Pro-Controller or individual Joy-Cons
        std::optional<NpadVibrationValue> vibrationRight; //!< Vibration for the right Joy-Con (Handheld/Pair) or right LRA in a Pro-Controller
        NpadControllerType type{};
        NpadConnectionState connectionState{};
        std::shared_ptr<kernel::type::KEvent> updateEvent; //!< This event is triggered on the controller's style changing

        NpadDevice(NpadManager &manager, NpadSection &section, NpadId id);

        /**
         * @brief This sets a Joy-Con's Assignment Mode
         * @param assignment The assignment mode to set this controller to
         */
        inline void SetAssignment(NpadJoyAssignment assignment) {
            section.header.assignment = assignment;
        }

        /**
         * @return The assignment mode of this Joy-Con
         */
        inline NpadJoyAssignment GetAssignment() {
            return section.header.assignment;
        }

        /**
         * @brief This connects this controller to the guest
         * @param newType The type of controller to connect as
         */
        void Connect(NpadControllerType newType);

        /**
         * @brief This disconnects this controller from the guest
         */
        void Disconnect();

        /**
         * @brief This changes the state of buttons to the specified state
         * @param mask A bit-field mask of all the buttons to change
         * @param pressed If the buttons were pressed or released
         */
        void SetButtonState(NpadButton mask, bool pressed);

        /**
         * @brief This sets the value of an axis to the specified value
         * @param axis The axis to set the value of
         * @param value The value to set
         */
        void SetAxisValue(NpadAxisId axis, i32 value);

        void Vibrate(bool isRight, const NpadVibrationValue &value);

        void Vibrate(const NpadVibrationValue &left, const NpadVibrationValue &right);
    };
}
