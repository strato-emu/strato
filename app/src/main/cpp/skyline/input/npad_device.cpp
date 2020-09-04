// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <jvm.h>
#include "npad_device.h"
#include "npad.h"

namespace skyline::input {
    NpadDevice::NpadDevice(NpadManager &manager, NpadSection &section, NpadId id) : manager(manager), section(section), id(id), updateEvent(std::make_shared<kernel::type::KEvent>(manager.state)) {}

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
        auto &lastEntry = info.state.at(info.header.currentEntry);

        info.header.timestamp = util::GetTimeTicks();
        info.header.entryCount = std::min(static_cast<u8>(info.header.entryCount + 1), constant::HidEntryCount);
        info.header.currentEntry = (info.header.currentEntry != constant::HidEntryCount - 1) ? info.header.currentEntry + 1 : 0;

        auto &entry = info.state.at(info.header.currentEntry);

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

        auto &entry = GetNextEntry(*controllerInfo);

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

        auto &defaultEntry = GetNextEntry(section.defaultController);
        if (pressed)
            defaultEntry.buttons.raw |= mask.raw;
        else
            defaultEntry.buttons.raw &= ~mask.raw;

        globalTimestamp++;
    }

    void NpadDevice::SetAxisValue(NpadAxisId axis, i32 value) {
        if (!connectionState.connected)
            return;

        auto &controllerEntry = GetNextEntry(*controllerInfo);
        auto &defaultEntry = GetNextEntry(section.defaultController);

        constexpr i16 threshold = std::numeric_limits<i16>::max() / 2; // A 50% deadzone for the stick buttons

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

    void NpadDevice::VibrateDevice(i8 vibrateIndex, const NpadVibrationValue &value) {
        std::array<jlong, 3> timings;
        std::array<jint, 3> amplitudes;

        jlong periodLow = 1000 / value.frequencyLow;
        jlong periodHigh = 1000 / value.frequencyHigh;

        jint amplitudeLow = value.amplitudeLow * 127;
        jint amplitudeHigh = value.amplitudeHigh * 127;

        if (amplitudeLow + amplitudeHigh == 0 || periodLow + periodHigh == 0) {
            manager.state.jvm->ClearVibrationDevice(vibrateIndex);
            return;
        }

        if (periodLow == periodHigh) {
            timings = {periodLow, periodHigh, 0};
            amplitudes = {std::min(amplitudeLow + amplitudeHigh, 255), 0, 0};
        } else if (periodLow < periodHigh) {
            timings = {periodLow, periodHigh - periodLow, periodHigh};
            amplitudes = {std::min(amplitudeLow + amplitudeHigh, 255), amplitudeHigh, 0};
        } else if (periodHigh < periodLow) {
            timings = {periodHigh, periodLow - periodHigh, periodLow};
            amplitudes = {std::min(amplitudeHigh + amplitudeLow, 255), amplitudeLow, 0};
        }

        manager.state.jvm->VibrateDevice(vibrateIndex, timings, amplitudes);
    }

    void NpadDevice::Vibrate(bool isRight, const NpadVibrationValue &value) {
        if (isRight)
            vibrationRight = value;
        else
            vibrationLeft = value;

        if (vibrationRight)
            Vibrate(vibrationLeft, *vibrationRight);
        else
            VibrateDevice(index, value);
    }

    void NpadDevice::Vibrate(const NpadVibrationValue &left, const NpadVibrationValue &right) {
        if (partnerIndex == -1) {
            std::array<jlong, 5> timings;
            std::array<jint, 5> amplitudes;

            std::array<std::pair<jlong, jint>, 4> vibrations{std::pair<jlong, jint>{1000 / left.frequencyLow, left.amplitudeLow * 64},
                                                             {1000 / left.frequencyHigh, left.amplitudeHigh * 64},
                                                             {1000 / right.frequencyLow, right.amplitudeLow * 64},
                                                             {1000 / right.frequencyHigh, right.amplitudeHigh * 64},
            };

            jlong totalTime{};
            std::sort(vibrations.begin(), vibrations.end(), [](const std::pair<jlong, jint> &a, const std::pair<jlong, jint> &b) {
                return a.first < b.first;
            });

            jint totalAmplitude{};
            for (const auto &vibration : vibrations)
                totalAmplitude += vibration.second;

            if (totalAmplitude == 0 || vibrations[3].first == 0) {
                manager.state.jvm->ClearVibrationDevice(index);
                return;
            }

            for (u8 i{0}; i < vibrations.size(); i++) {
                const auto &vibration = vibrations[i];

                auto time = vibration.first - totalTime;
                timings[i] = time;
                totalTime += time;

                amplitudes[i] = std::min(totalAmplitude, 255);
                totalAmplitude -= vibration.second;
            }

            timings[4] = totalTime;
            amplitudes[4] = 0;

            manager.state.jvm->VibrateDevice(index, timings, amplitudes);
        } else {
            VibrateDevice(index, left);
            VibrateDevice(partnerIndex, right);
        }
    }
}
