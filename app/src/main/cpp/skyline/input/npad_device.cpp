// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "npad_device.h"

namespace skyline::input {
    NpadDevice::NpadDevice(NpadSection &section, NpadId id) : section(section), id(id) {}

    void NpadDevice::Connect(NpadControllerType type) {
        section.header.type = NpadControllerType::None;
        section.deviceType.raw = 0;
        section.buttonProperties.raw = 0;

        connectionState.raw = 0;
        connectionState.connected = true;

        switch (type) {
            case NpadControllerType::Handheld:
                section.header.type = NpadControllerType::Handheld;
                section.deviceType.handheldLeft = true;
                section.deviceType.handheldRight = true;
                section.header.assignment = NpadJoyAssignment::Dual;
                section.systemProperties.ABXYButtonOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;

                connectionState.handheld = true;
                connectionState.leftJoyconConnected = true;
                connectionState.rightJoyconConnected = true;
                connectionState.leftJoyconHandheld = true;
                connectionState.rightJoyconHandheld = true;
                break;
            case NpadControllerType::ProController:
                section.header.type = NpadControllerType::ProController;
                section.deviceType.fullKey = true;
                section.deviceType.joyconRight = true;
                section.header.assignment = NpadJoyAssignment::Single;
                section.systemProperties.ABXYButtonOriented = true;
                section.systemProperties.plusButtonCapability = true;
                section.systemProperties.minusButtonCapability = true;
                break;
            default:
                throw exception("Unsupported controller type: {}", type);
        }

        controllerType = type;

        section.header.singleColorStatus = NpadColorReadStatus::Success;
        section.header.singleColor = {0, 0};

        section.header.dualColorStatus = NpadColorReadStatus::Success;
        section.header.leftColor = {0, 0}; //TODO: make these configurable
        section.header.rightColor = {0, 0};

        section.batteryLevel[0] = NpadBatteryLevel::Full;
        section.batteryLevel[1] = NpadBatteryLevel::Full;
        section.batteryLevel[2] = NpadBatteryLevel::Full;

        SetButtonState(NpadButton{}, NpadButtonState::Released);
    }

    void NpadDevice::Disconnect() {
        connectionState.connected = false;

        GetNextEntry(GetControllerInfo());
        GetNextEntry(section.systemExtController);
    }

    NpadControllerInfo &NpadDevice::GetControllerInfo() {
        switch (controllerType) {
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
                throw exception("Cannot find corresponding section for ControllerType: {}", controllerType);
        }
    }

    NpadControllerState &NpadDevice::GetNextEntry(NpadControllerInfo &info) {
        auto &lastEntry = info.state.at(info.header.currentEntry);

        info.header.entryCount = constant::HidEntryCount;
        info.header.maxEntry = constant::HidEntryCount - 1;
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
        if (!connectionState.connected)
            return;

        for (NpadControllerInfo &controllerInfo : {std::ref(GetControllerInfo()), std::ref(section.systemExtController)}) {
            auto &entry = GetNextEntry(controllerInfo);

            if (state == NpadButtonState::Pressed)
                entry.buttons.raw |= mask.raw;
            else
                entry.buttons.raw &= ~(mask.raw);
        }

        globalTimestamp++;
    }

    void NpadDevice::SetAxisValue(NpadAxisId axis, i32 value) {
        if (!connectionState.connected)
            return;

        for (NpadControllerInfo &controllerInfo : {std::ref(GetControllerInfo()), std::ref(section.systemExtController)}) {
            auto &entry = GetNextEntry(controllerInfo);

            switch (axis) {
                case NpadAxisId::LX:
                    entry.leftX = value;
                    break;
                case NpadAxisId::LY:
                    entry.leftY = -value; // Invert Y axis
                    break;
                case NpadAxisId::RX:
                    entry.rightX = value;
                    break;
                case NpadAxisId::RY:
                    entry.rightY = -value; // Invert Y axis
                    break;
            }
        }

        globalTimestamp++;
    }
}
