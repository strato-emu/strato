// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "npad_device.h"
#include "npad.h"

namespace skyline::input {
    NpadDevice::NpadDevice(NpadManager &manager, NpadSection &section, NpadId id) : manager(manager), section(section), id(id) {}

    void NpadDevice::Connect(NpadControllerType type) {
        section.header.type = NpadControllerType::None;
        section.deviceType.raw = 0;
        section.buttonProperties.raw = 0;

        connectionState.raw = 0;
        connectionState.connected = true;

        switch (type) {
            case NpadControllerType::ProController:
                section.header.type = NpadControllerType::ProController;
                section.deviceType.fullKey = true;

                section.systemProperties.directionalButtonsSupported = true;
                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;

                connectionState.connected = true;
                break;

            case NpadControllerType::Handheld:
                section.header.type = NpadControllerType::Handheld;
                section.deviceType.handheldLeft = true;
                section.deviceType.handheldRight = true;

                section.systemProperties.directionalButtonsSupported = true;
                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;

                connectionState.handheld = true;
                connectionState.leftJoyconHandheld = true;
                connectionState.rightJoyconHandheld = true;
                break;

            case NpadControllerType::JoyconDual:
                section.header.type = NpadControllerType::JoyconDual;
                section.deviceType.joyconLeft = true;
                section.deviceType.joyconRight = true;
                section.header.assignment = NpadJoyAssignment::Dual;

                section.systemProperties.directionalButtonsSupported = true;
                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;

                connectionState.connected = true;
                connectionState.leftJoyconConnected = true;
                connectionState.rightJoyconConnected = true;
                break;

            case NpadControllerType::JoyconLeft:
                section.header.type = NpadControllerType::JoyconLeft;
                section.deviceType.joyconLeft = true;
                section.header.assignment = NpadJoyAssignment::Single;

                section.systemProperties.directionalButtonsSupported = true;
                section.systemProperties.slSrButtonOriented = true;
                section.systemProperties.minusButtonCapability = true;

                connectionState.connected = true;
                connectionState.leftJoyconConnected = true;
                break;

            case NpadControllerType::JoyconRight:
                section.header.type = NpadControllerType::JoyconRight;
                section.deviceType.joyconRight = true;
                section.header.assignment = NpadJoyAssignment::Single;

                section.systemProperties.abxyButtonsOriented = true;
                section.systemProperties.slSrButtonOriented = true;
                section.systemProperties.plusButtonCapability = true;

                connectionState.connected = true;
                connectionState.rightJoyconConnected = true;
                break;

            default:
                throw exception("Unsupported controller type: {}", type);
        }

        switch (type) {
            case NpadControllerType::ProController:
            case NpadControllerType::JoyconLeft:
            case NpadControllerType::JoyconRight:
                section.header.singleColorStatus = NpadColorReadStatus::Success;
                section.header.dualColorStatus = NpadColorReadStatus::Disconnected;
                section.header.singleColor = {0, 0};
                break;

            case NpadControllerType::Handheld:
            case NpadControllerType::JoyconDual:
                section.header.singleColorStatus = NpadColorReadStatus::Disconnected;
                section.header.dualColorStatus = NpadColorReadStatus::Success;
                section.header.leftColor = {0, 0};
                section.header.rightColor = {0, 0};
                break;

            case NpadControllerType::None:
                break;
        }

        section.singleBatteryLevel = NpadBatteryLevel::Full;
        section.leftBatteryLevel = NpadBatteryLevel::Full;
        section.rightBatteryLevel = NpadBatteryLevel::Full;

        this->type = type;
        controllerInfo = &GetControllerInfo();

        SetButtonState(NpadButton{}, NpadButtonState::Released);
    }

    void NpadDevice::Disconnect() {
        section = {};
        explicitAssignment = false;
        globalTimestamp = 0;

        type = NpadControllerType::None;
        controllerInfo = nullptr;
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

        info.header.currentEntry = (info.header.currentEntry != constant::HidEntryCount - 1) ? info.header.currentEntry + 1 : 0;
        info.header.timestamp = util::GetTimeTicks();

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

    void NpadDevice::SetButtonState(NpadButton mask, NpadButtonState state) {
        if (!connectionState.connected || !controllerInfo)
            return;

        auto &entry = GetNextEntry(*controllerInfo);

        if (state == NpadButtonState::Pressed)
            entry.buttons.raw |= mask.raw;
        else
            entry.buttons.raw &= ~mask.raw;

        if ((type == NpadControllerType::JoyconLeft || type == NpadControllerType::JoyconRight) && manager.orientation == NpadJoyOrientation::Horizontal) {
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

        for (NpadControllerState& controllerEntry : {std::ref(GetNextEntry(section.defaultController)), std::ref(GetNextEntry(section.digitalController))})
            if (state == NpadButtonState::Pressed)
                controllerEntry.buttons.raw |= mask.raw;
            else
                controllerEntry.buttons.raw &= ~mask.raw;

        globalTimestamp++;
    }

    void NpadDevice::SetAxisValue(NpadAxisId axis, i32 value) {
        if (!connectionState.connected || !controllerInfo)
            return;

        auto &controllerEntry = GetNextEntry(*controllerInfo);
        auto& defaultEntry = GetNextEntry(section.defaultController);

        if (manager.orientation == NpadJoyOrientation::Vertical && (type != NpadControllerType::JoyconLeft && type != NpadControllerType::JoyconRight)) {
            switch (axis) {
                case NpadAxisId::LX:
                    controllerEntry.leftX = value;
                    defaultEntry.leftX = value;
                    break;
                case NpadAxisId::LY:
                    controllerEntry.leftY = value;
                    defaultEntry.leftY = value;
                    break;
                case NpadAxisId::RX:
                    controllerEntry.rightX = value;
                    defaultEntry.rightX = value;
                    break;
                case NpadAxisId::RY:
                    controllerEntry.rightY = value;
                    defaultEntry.rightY = value;
                    break;
            }
        } else {
            switch (axis) {
                case NpadAxisId::LX:
                    controllerEntry.leftX = value;
                    defaultEntry.leftY = value;
                    break;
                case NpadAxisId::LY:
                    controllerEntry.leftY = value;
                    defaultEntry.leftX = -value;
                    break;
                case NpadAxisId::RX:
                    controllerEntry.rightX = value;
                    defaultEntry.rightY = value;
                    break;
                case NpadAxisId::RY:
                    controllerEntry.rightY = value;
                    defaultEntry.rightX = -value;
                    break;
            }
        }

        auto& digitalEntry = GetNextEntry(section.digitalController);
        constexpr i16 threshold = 3276; // A 10% deadzone for the stick

        digitalEntry.buttons.leftStickUp = defaultEntry.leftY >= threshold;
        digitalEntry.buttons.leftStickDown = defaultEntry.leftY <= -threshold;
        digitalEntry.buttons.leftStickLeft = defaultEntry.leftX <= -threshold;
        digitalEntry.buttons.leftStickRight = defaultEntry.leftX >= threshold;

        digitalEntry.buttons.rightStickUp = defaultEntry.rightY >= threshold;
        digitalEntry.buttons.rightStickDown = defaultEntry.rightY <= -threshold;
        digitalEntry.buttons.rightStickLeft = defaultEntry.rightX <= -threshold;
        digitalEntry.buttons.rightStickRight = defaultEntry.rightX >= threshold;

        globalTimestamp++;
    }
}
