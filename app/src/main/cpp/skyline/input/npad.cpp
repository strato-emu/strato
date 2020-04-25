// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "npad.h"

namespace skyline::input::npad {
    u32 NpadIdToIndex(NpadId id) {
        switch (id) {
            case NpadId::Unknown:
                return 8;
            case NpadId::Handheld:
                return 9;
            default:
                return static_cast<u32>(id);
        }
    }

    NpadId IndexToNpadId(u32 index) {
        switch (index) {
            case 8:
                return NpadId::Unknown;
            case 9:
                return NpadId::Handheld;
            default:
                return static_cast<NpadId>(index);
        }
    }

    NpadDevice::NpadDevice(NpadSection &shmemSection, NpadId id) : shmemSection(shmemSection), id(id) {}

    void NpadDevice::Disconnect() {
        connectionState.connected = false;
    }

    void NpadDevice::Connect(NpadControllerType type) {
        shmemSection.header.styles.raw = 0;
        shmemSection.deviceType.raw = 0;
        shmemSection.properties.raw = 0;

        connectionState.raw = 0;
        connectionState.connected = true;

        switch (type) {
            case NpadControllerType::Handheld:
                shmemSection.header.styles.joyconHandheld = true;
                shmemSection.deviceType.handheld = true;
                shmemSection.deviceType.handheldLeft = true;
                shmemSection.deviceType.handheldRight = true;
                shmemSection.header.assignment = NpadJoyAssignment::Dual;
                shmemSection.properties.ABXYButtonOriented = true;
                shmemSection.properties.plusButtonCapability = true;
                shmemSection.properties.minusButtonCapability = true;

                connectionState.handheld = true;
                connectionState.leftJoyconConnected = true;
                connectionState.rightJoyconConnected = true;
                connectionState.leftJoyconHandheld = true;
                connectionState.rightJoyconHandheld = true;
                break;
            case NpadControllerType::ProController:
                shmemSection.header.styles.proController = true;
                shmemSection.deviceType.fullKey = true;
                shmemSection.deviceType.joyconRight = true;
                shmemSection.header.assignment = NpadJoyAssignment::Single;
                shmemSection.properties.ABXYButtonOriented = true;
                shmemSection.properties.plusButtonCapability = true;
                shmemSection.properties.minusButtonCapability = true;
                break;
            default:
                throw exception("Unsupported controller type: {}", type);
        }

        controllerType = type;

        shmemSection.header.singleColourStatus = NpadColourReadStatus::Success;
        shmemSection.header.singleColour = {0, 0};

        shmemSection.header.dualColourStatus = NpadColourReadStatus::Success;
        shmemSection.header.leftColour = {0, 0}; //TODO: make these configurable
        shmemSection.header.rightColour = {0, 0};

        shmemSection.batteryLevel[0] = constant::NpadBatteryFull;
        shmemSection.batteryLevel[1] = constant::NpadBatteryFull;
        shmemSection.batteryLevel[2] = constant::NpadBatteryFull;

        // Set controllers initial state
        SetButtonState(NpadButton{}, NpadButtonState::Released);

        //TODO: signal npad event
    }

    NpadControllerInfo &NpadDevice::GetControllerDeviceInfo() {
        switch (controllerType) {
            case NpadControllerType::Handheld:
                return shmemSection.handheldController;
            case NpadControllerType::ProController:
            default:
                return shmemSection.fullKeyController;
        }
    }

    void NpadDevice::UpdateHeaders(NpadControllerInfo &controller, uint lastStateEntryIndex) {
        controller.header.entryCount = constant::HidEntryCount;
        controller.header.maxEntry = constant::HidEntryCount - 1;
        controller.header.currentEntry = stateEntryIndex;
        controller.header.timestamp = util::GetTimeNs();

        memcpy(reinterpret_cast<void *>(&controller.state.at(stateEntryIndex)), reinterpret_cast<void *>(&controller.state.at(lastStateEntryIndex)), sizeof(NpadControllerState));

        controller.state.at(stateEntryIndex).globalTimestamp++;
        controller.state.at(stateEntryIndex).localTimestamp++;
        controller.state.at(stateEntryIndex).status.raw = connectionState.raw;
    }

    void NpadDevice::SetButtonState(NpadButton button, NpadButtonState state) {
        NpadControllerInfo &controllerDeviceInfo = GetControllerDeviceInfo();
        uint lastStateEntryIndex = stateEntryIndex;
        stateEntryIndex = (stateEntryIndex + 1) % constant::HidEntryCount;

        for (NpadControllerInfo &controllerInfo : {std::ref(controllerDeviceInfo), std::ref(shmemSection.systemExtController)}) {
            UpdateHeaders(controllerInfo, lastStateEntryIndex);

            if (state == NpadButtonState::Pressed)
                controllerInfo.state.at(stateEntryIndex).controller.buttons.raw |= button.raw;
            else
                controllerInfo.state.at(stateEntryIndex).controller.buttons.raw &= ~(button.raw);
        }
    }

    void NpadDevice::SetAxisValue(NpadAxisId axis, int value) {
        NpadControllerInfo controllerDeviceInfo = GetControllerDeviceInfo();
        uint lastStateEntryIndex = stateEntryIndex;
        stateEntryIndex = (stateEntryIndex + 1) % constant::HidEntryCount;

        for (NpadControllerInfo &controllerInfo : {std::ref(controllerDeviceInfo), std::ref(shmemSection.systemExtController)}) {
            UpdateHeaders(controllerInfo, lastStateEntryIndex);

            switch (axis) {
                case NpadAxisId::LX:
                    controllerInfo.state.at(stateEntryIndex).controller.leftX = value;
                    break;
                case NpadAxisId::LY:
                    controllerInfo.state.at(stateEntryIndex).controller.leftY = -value; // Invert Y axis
                    break;
                case NpadAxisId::RX:
                    controllerInfo.state.at(stateEntryIndex).controller.rightX = value;
                    break;
                case NpadAxisId::RY:
                    controllerInfo.state.at(stateEntryIndex).controller.rightY = -value; // Invert Y axis
                    break;
            }
        }
    }

    CommonNpad::CommonNpad(const DeviceState &state) : state(state) {}

    void CommonNpad::Activate() {
        // Always mark controllers we support as supported
        if (supportedStyles.raw == 0) {
            supportedStyles.proController = true;
            supportedStyles.joyconHandheld = true;
        }

        for (uint i = 0; i < constant::NpadCount; i++) {
            bool shouldConnect = (i == 0); // P1

            //TODO: Read this as well as controller type from settings based off of NpadID
            if (shouldConnect) {
                if (state.settings->GetBool("operation_mode"))
                    state.input->npad.at(i)->Connect(NpadControllerType::Handheld);
                else
                    state.input->npad.at(i)->Connect(NpadControllerType::ProController);
            }
        }
    }
}
