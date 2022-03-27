// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <jvm.h>
#include "npad_device.h"
#include "npad.h"

namespace skyline::input {
    NpadDevice::NpadDevice(NpadManager &manager, NpadSection &section, NpadId id)
        : manager(manager),
          section(section),
          id(id),
          updateEvent(std::make_shared<kernel::type::KEvent>(manager.state, false)) {}

    void NpadDevice::Connect(NpadControllerType newType) {
        if (type == newType) {
            if (type == NpadControllerType::JoyconLeft || type == NpadControllerType::JoyconRight) {
                switch (manager.orientation) {
                    case NpadJoyOrientation::Vertical:
                        section.systemProperties.abxyButtonsOriented = true;
                        section.systemProperties.slSrButtonOriented = false;
                        break;
                    case NpadJoyOrientation::Horizontal:
                        section.systemProperties.abxyButtonsOriented = false;
                        section.systemProperties.slSrButtonOriented = true;
                        break;
                }
            }
            return;
        }

        section = {};
        controllerInfo = nullptr;

        connectionState = {.connected = true};

        switch (newType) {
            case NpadControllerType::ProController:
                section.header.type = NpadControllerType::ProController;
                section.deviceType.fullKey = true;

                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;

                connectionState.handheld = true;
                break;

            case NpadControllerType::Handheld:
                section.header.type = NpadControllerType::Handheld;
                section.deviceType.handheldLeft = true;
                section.deviceType.handheldRight = true;

                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;
                section.systemProperties.directionalButtonsSupported = true;

                connectionState.handheld = true;
                connectionState.leftJoyconConnected = true;
                connectionState.leftJoyconHandheld = true;
                connectionState.rightJoyconConnected = true;
                connectionState.rightJoyconHandheld = true;
                break;

            case NpadControllerType::JoyconDual:
                section.header.type = NpadControllerType::JoyconDual;
                section.deviceType.joyconLeft = true;
                section.deviceType.joyconRight = true;
                section.header.assignment = NpadJoyAssignment::Dual;

                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;
                section.systemProperties.directionalButtonsSupported = true;

                connectionState.leftJoyconConnected = true;
                connectionState.rightJoyconConnected = true;
                break;

            case NpadControllerType::JoyconLeft:
                section.header.type = NpadControllerType::JoyconLeft;
                section.deviceType.joyconLeft = true;
                section.header.assignment = NpadJoyAssignment::Single;

                if (manager.orientation == NpadJoyOrientation::Vertical)
                    section.systemProperties.abxyButtonsOriented = true;
                else if (manager.orientation == NpadJoyOrientation::Horizontal)
                    section.systemProperties.slSrButtonOriented = true;

                section.systemProperties.minusButtonCapability = true;
                section.systemProperties.directionalButtonsSupported = true;

                connectionState.leftJoyconConnected = true;
                break;

            case NpadControllerType::JoyconRight:
                section.header.type = NpadControllerType::JoyconRight;
                section.deviceType.joyconRight = true;
                section.header.assignment = NpadJoyAssignment::Single;

                if (manager.orientation == NpadJoyOrientation::Vertical)
                    section.systemProperties.abxyButtonsOriented = true;
                else if (manager.orientation == NpadJoyOrientation::Horizontal)
                    section.systemProperties.slSrButtonOriented = true;

                section.systemProperties.slSrButtonOriented = true;
                section.systemProperties.plusButtonCapability = true;

                connectionState.rightJoyconConnected = true;
                break;

            default:
                throw exception("Unsupported controller type: {}", newType);
        }

        switch (newType) {
            case NpadControllerType::ProController:
            case NpadControllerType::JoyconLeft:
            case NpadControllerType::JoyconRight:
                section.header.singleColorStatus = NpadColorReadStatus::Success;
                if (newType == NpadControllerType::ProController)
                    section.header.singleColor = {0xFF2D2D2D, 0xFFE6E6E6}; // Normal Pro-Controller
                else
                    section.header.singleColor = {0x4655F5, 0x00000A}; // Blue Joy-Con (https://switchbrew.org/wiki/Joy-Con#Colors)
                break;

            case NpadControllerType::Handheld:
            case NpadControllerType::JoyconDual:
                section.header.dualColorStatus = NpadColorReadStatus::Success;
                section.header.leftColor = {0x4655F5, 0x00000A};
                section.header.rightColor = {0x4655F5, 0x00000A};

                section.header.singleColorStatus = NpadColorReadStatus::Success; // Single color is also written for dual controllers
                section.header.singleColor = section.header.leftColor;           // and is set to the color of the left JC
                break;
            case NpadControllerType::Gamecube:
            case NpadControllerType::None:
                break;
        }

        section.singleBatteryLevel = NpadBatteryLevel::Full;
        section.leftBatteryLevel = NpadBatteryLevel::Full;
        section.rightBatteryLevel = NpadBatteryLevel::Full;

        type = newType;
        controllerInfo = &GetControllerInfo();

        GetNextEntry(*controllerInfo);
        GetNextEntry(section.defaultController);
        globalTimestamp++;

        updateEvent->Signal();
    }

    void NpadDevice::Disconnect() {
        if (type == NpadControllerType::None)
            return;

        section = {};
        globalTimestamp = 0;

        index = -1;
        partnerIndex = -1;

        type = NpadControllerType::None;
        controllerInfo = nullptr;

        updateEvent->Signal();
    }

    NpadControllerInfo &NpadDevice::GetControllerInfo() {
        switch (type) {
            case NpadControllerType::ProController:
                return section.fullKeyController;
            case NpadControllerType::Handheld:
                return section.handheldController;
            case NpadControllerType::JoyconDual:
                return section.dualController;
            case NpadControllerType::JoyconLeft:
                return section.leftController;
            case NpadControllerType::JoyconRight:
                return section.rightController;
            default:
                throw exception("Cannot find corresponding section for ControllerType: {}", type);
        }
    }

    NpadControllerState &NpadDevice::GetNextEntry(NpadControllerInfo &info) {
        auto &lastEntry{info.state.at(info.header.currentEntry)};

        info.header.timestamp = util::GetTimeTicks();
        info.header.entryCount = std::min(static_cast<u8>(info.header.entryCount + 1), constant::HidEntryCount);
        info.header.maxEntry = info.header.entryCount;
        info.header.currentEntry = (info.header.currentEntry != constant::HidEntryCount - 1) ? info.header.currentEntry + 1 : 0;

        auto &entry{info.state.at(info.header.currentEntry)};

        entry.globalTimestamp = globalTimestamp;
        entry.localTimestamp = lastEntry.localTimestamp + 1;
        entry.buttons = lastEntry.buttons;
        entry.leftX = lastEntry.leftX;
        entry.leftY = lastEntry.leftY;
        entry.rightX = lastEntry.rightX;
        entry.rightY = lastEntry.rightY;
        entry.status.raw = connectionState.raw;

        return entry;
    }

    void NpadDevice::SetButtonState(NpadButton mask, bool pressed) {
        if (!connectionState.connected)
            return;

        auto &entry{GetNextEntry(*controllerInfo)};

        if (pressed)
            entry.buttons.raw |= mask.raw;
        else
            entry.buttons.raw &= ~mask.raw;

        if (manager.orientation == NpadJoyOrientation::Horizontal && (type == NpadControllerType::JoyconLeft || type == NpadControllerType::JoyconRight)) {
            NpadButton orientedMask{};

            if (mask.dpadUp)
                orientedMask.dpadLeft = true;
            if (mask.dpadDown)
                orientedMask.dpadRight = true;
            if (mask.dpadLeft)
                orientedMask.dpadDown = true;
            if (mask.dpadRight)
                orientedMask.dpadUp = true;

            if (mask.leftSl || mask.rightSl)
                orientedMask.l = true;
            if (mask.leftSr || mask.rightSr)
                orientedMask.r = true;

            orientedMask.a = mask.a;
            orientedMask.b = mask.b;
            orientedMask.x = mask.x;
            orientedMask.y = mask.y;
            orientedMask.leftStick = mask.leftStick;
            orientedMask.rightStick = mask.rightStick;
            orientedMask.plus = mask.plus;
            orientedMask.minus = mask.minus;
            orientedMask.leftSl = mask.leftSl;
            orientedMask.leftSr = mask.leftSr;
            orientedMask.rightSl = mask.rightSl;
            orientedMask.rightSr = mask.rightSr;

            mask = orientedMask;
        }

        auto &defaultEntry{GetNextEntry(section.defaultController)};
        if (pressed)
            defaultEntry.buttons.raw |= mask.raw;
        else
            defaultEntry.buttons.raw &= ~mask.raw;

        globalTimestamp++;
    }

    void NpadDevice::SetAxisValue(NpadAxisId axis, i32 value) {
        if (!connectionState.connected)
            return;

        auto &controllerEntry{GetNextEntry(*controllerInfo)};
        auto &defaultEntry{GetNextEntry(section.defaultController)};

        constexpr i16 threshold{std::numeric_limits<i16>::max() / 2}; // A 50% deadzone for the stick buttons

        if (manager.orientation == NpadJoyOrientation::Vertical || (type != NpadControllerType::JoyconLeft && type != NpadControllerType::JoyconRight)) {
            switch (axis) {
                case NpadAxisId::LX:
                    controllerEntry.leftX = value;
                    defaultEntry.leftX = value;

                    controllerEntry.buttons.leftStickLeft = controllerEntry.leftX <= -threshold;
                    defaultEntry.buttons.leftStickLeft = controllerEntry.buttons.leftStickLeft;

                    controllerEntry.buttons.leftStickRight = controllerEntry.leftX >= threshold;
                    defaultEntry.buttons.leftStickRight = controllerEntry.buttons.leftStickRight;
                    break;
                case NpadAxisId::LY:
                    controllerEntry.leftY = value;
                    defaultEntry.leftY = value;

                    defaultEntry.buttons.leftStickUp = controllerEntry.buttons.leftStickUp;
                    controllerEntry.buttons.leftStickUp = controllerEntry.leftY >= threshold;

                    controllerEntry.buttons.leftStickDown = controllerEntry.leftY <= -threshold;
                    defaultEntry.buttons.leftStickDown = controllerEntry.buttons.leftStickDown;
                    break;
                case NpadAxisId::RX:
                    controllerEntry.rightX = value;
                    defaultEntry.rightX = value;

                    controllerEntry.buttons.rightStickLeft = controllerEntry.rightX <= -threshold;
                    defaultEntry.buttons.rightStickLeft = controllerEntry.buttons.rightStickLeft;

                    controllerEntry.buttons.rightStickRight = controllerEntry.rightX >= threshold;
                    defaultEntry.buttons.rightStickRight = controllerEntry.buttons.rightStickRight;
                    break;
                case NpadAxisId::RY:
                    controllerEntry.rightY = value;
                    defaultEntry.rightY = value;

                    controllerEntry.buttons.rightStickUp = controllerEntry.rightY >= threshold;
                    defaultEntry.buttons.rightStickUp = controllerEntry.buttons.rightStickUp;

                    controllerEntry.buttons.rightStickDown = controllerEntry.rightY <= -threshold;
                    defaultEntry.buttons.rightStickDown = controllerEntry.buttons.rightStickDown;
                    break;
            }
        } else {
            switch (axis) {
                case NpadAxisId::LX:
                    controllerEntry.leftY = value;
                    controllerEntry.buttons.leftStickUp = controllerEntry.leftY >= threshold;
                    controllerEntry.buttons.leftStickDown = controllerEntry.leftY <= -threshold;

                    defaultEntry.leftX = value;
                    defaultEntry.buttons.leftStickLeft = defaultEntry.leftX <= -threshold;
                    defaultEntry.buttons.leftStickRight = defaultEntry.leftX >= threshold;
                    break;
                case NpadAxisId::LY:
                    controllerEntry.leftX = -value;
                    controllerEntry.buttons.leftStickLeft = controllerEntry.leftX <= -threshold;
                    controllerEntry.buttons.leftStickRight = controllerEntry.leftX >= threshold;

                    defaultEntry.leftY = value;
                    defaultEntry.buttons.leftStickUp = defaultEntry.leftY >= threshold;
                    defaultEntry.buttons.leftStickDown = defaultEntry.leftY <= -threshold;
                    break;
                case NpadAxisId::RX:
                    controllerEntry.rightY = value;
                    controllerEntry.buttons.rightStickUp = controllerEntry.rightY >= threshold;
                    controllerEntry.buttons.rightStickDown = controllerEntry.rightY <= -threshold;

                    defaultEntry.rightX = value;
                    defaultEntry.buttons.rightStickLeft = defaultEntry.rightX <= -threshold;
                    defaultEntry.buttons.rightStickRight = defaultEntry.rightX >= threshold;
                    break;
                case NpadAxisId::RY:
                    controllerEntry.rightX = -value;
                    controllerEntry.buttons.rightStickLeft = controllerEntry.rightX <= -threshold;
                    controllerEntry.buttons.rightStickRight = controllerEntry.rightX >= threshold;

                    defaultEntry.rightY = value;
                    defaultEntry.buttons.rightStickUp = defaultEntry.rightY >= threshold;
                    defaultEntry.buttons.rightStickDown = defaultEntry.rightY <= -threshold;
                    break;
            }
        }

        globalTimestamp++;
    }

    constexpr jlong MsInSecond{1000}; //!< The amount of milliseconds in a single second of time
    constexpr jint AmplitudeMax{std::numeric_limits<u8>::max()}; //!< The maximum amplitude for Android Vibration APIs

    struct VibrationInfo {
        jlong period;
        jint amplitude;
        jlong start; //!< The timestamp to (re)start the vibration at
        jlong end; //!< The timestamp to end the vibration at

        VibrationInfo(float frequency, float amplitude)
            : period(static_cast<jlong>(MsInSecond / frequency)),
              amplitude(static_cast<jint>(amplitude)),
              start(0), end(period) {}
    };

    template<size_t Size>
    void VibrateDevice(const std::shared_ptr<JvmManager> &jvm, i8 deviceIndex, std::array<VibrationInfo, Size> vibrations) {
        jint totalAmplitude{};
        for (const auto &vibration : vibrations)
            totalAmplitude += vibration.amplitude;
        if (totalAmplitude == 0) {
            jvm->ClearVibrationDevice(deviceIndex); // If a null vibration was submitted then we just clear vibrations on the device
            return;
        }

        // We output an approximation of the combined + linearized vibration data into these arrays, larger arrays would allow for more accurate reproduction of data
        std::array<jlong, 50> timings;
        std::array<jint, 50> amplitudes;

        // We are essentially unrolling the bands into a linear sequence, due to the data not being always linearizable there will be inaccuracies at the ends unless there's a pattern that's repeatable which will happen when all band's frequencies are factors of each other
        jint currentAmplitude{}; //!< The accumulated amplitude from adding up and subtracting the amplitude of individual bands
        jlong currentTime{}; //!< The accumulated time passed by adding up all the periods prior to the current vibration cycle
        size_t index{};
        for (; index < timings.size(); index++) {
            jlong cyclePeriod{}; //!< The length of this cycle, calculated as the largest period with the same amplitude
            size_t bandStartCount{}; //!< The amount of bands that start their vibration cycles in this time slot

            for (size_t vibrationIndex{}; vibrationIndex < vibrations.size(); vibrationIndex++) {
                // Iterate over every band to calculate the amplitude for this time slot
                VibrationInfo &vibration{vibrations[vibrationIndex]};
                if (currentTime <= vibration.start) {
                    // If the time to start has arrived then start the vibration
                    vibration.end = vibration.start + vibration.period;
                    currentAmplitude += vibration.amplitude;
                    auto vibrationPeriodLeft{vibration.end - currentTime};
                    cyclePeriod = cyclePeriod ? std::min(vibrationPeriodLeft, cyclePeriod) : vibrationPeriodLeft;

                    bandStartCount++;
                } else if (currentTime <= vibration.end) {
                    // If the time to end the vibration has arrived then end it
                    vibration.start = vibration.end + vibration.period;
                    currentAmplitude -= vibration.amplitude;
                    auto vibrationPeriodLeft{vibration.start - currentTime};
                    cyclePeriod = cyclePeriod ? std::min(vibrationPeriodLeft, cyclePeriod) : vibrationPeriodLeft;
                }
            }

            if (index && bandStartCount == vibrations.size())
                break; // If all bands start again at this point then we can end the pattern here and just loop over the pattern

            currentTime += cyclePeriod;
            timings[index] = cyclePeriod;

            amplitudes[index] = std::min(currentAmplitude, AmplitudeMax);
        }

        jvm->VibrateDevice(deviceIndex, span(timings.begin(), timings.begin() + index), span(amplitudes.begin(), amplitudes.begin() + index));
    }

    void VibrateDevice(const std::shared_ptr<JvmManager> &jvm, i8 index, const NpadVibrationValue &value) {
        std::array<VibrationInfo, 2> vibrations{
            VibrationInfo{value.frequencyLow, value.amplitudeLow * (AmplitudeMax / 2)},
            VibrationInfo{value.frequencyHigh, value.amplitudeHigh * (AmplitudeMax / 2)},
        };
        VibrateDevice(jvm, index, vibrations);
    }

    void NpadDevice::Vibrate(const NpadVibrationValue &left, const NpadVibrationValue &right) {
        if (vibrationLeft == left && vibrationRight && (*vibrationRight) == right)
            return;

        vibrationLeft = left;
        vibrationRight = right;

        if (partnerIndex == NpadDevice::NullIndex) {
            std::array<VibrationInfo, 4> vibrations{
                VibrationInfo{left.frequencyLow, left.amplitudeLow * (AmplitudeMax / 4)},
                VibrationInfo{left.frequencyHigh, left.amplitudeHigh * (AmplitudeMax / 4)},
                VibrationInfo{right.frequencyLow, right.amplitudeLow * (AmplitudeMax / 4)},
                VibrationInfo{right.frequencyHigh, right.amplitudeHigh * (AmplitudeMax / 4)},
            };
            VibrateDevice(manager.state.jvm, index, vibrations);
        } else {
            VibrateDevice(manager.state.jvm, index, left);
            VibrateDevice(manager.state.jvm, partnerIndex, right);
        }
    }

    void NpadDevice::VibrateSingle(bool isRight, const NpadVibrationValue &value) {
        if (isRight) {
            if (vibrationRight && (*vibrationRight) == value)
                return;
            vibrationRight = value;
        } else {
            if (vibrationLeft == value)
                return;
            vibrationLeft = value;
        }

        if (vibrationRight)
            Vibrate(vibrationLeft, *vibrationRight);
        else
            VibrateDevice(manager.state.jvm, index, value);
    }
}
