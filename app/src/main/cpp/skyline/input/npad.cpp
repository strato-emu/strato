// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "npad.h"

namespace skyline::input {
    NpadManager::NpadManager(const DeviceState &state, input::HidSharedMemory *hid) : state(state), npads
        {NpadDevice{*this, hid->npad[0], NpadId::Player1}, {*this, hid->npad[1], NpadId::Player2},
         {*this, hid->npad[2], NpadId::Player3}, {*this, hid->npad[3], NpadId::Player4},
         {*this, hid->npad[4], NpadId::Player5}, {*this, hid->npad[5], NpadId::Player6},
         {*this, hid->npad[6], NpadId::Player7}, {*this, hid->npad[7], NpadId::Player8},
         {*this, hid->npad[8], NpadId::Handheld}, {*this, hid->npad[9], NpadId::Unknown},
        } { Activate(); /* NPads are activated by default, certain homebrew is reliant on this behavior */ }

    void NpadManager::Update() {
        std::scoped_lock guard{mutex};

        if (!activated)
            return;

        for (auto &controller : controllers)
            controller.device = nullptr;

        for (auto &id : supportedIds) {
            if (id == NpadId::Unknown || !IsNpadIdValid(id))
                continue;

            auto &device{at(id)};

            for (auto &controller : controllers) {
                if (controller.device)
                    continue;

                NpadStyleSet style{};
                if (id != NpadId::Handheld) {
                    if (controller.type == NpadControllerType::ProController)
                        style.proController = true;
                    else if (controller.type == NpadControllerType::JoyconLeft)
                        style.joyconLeft = true;
                    else if (controller.type == NpadControllerType::JoyconRight)
                        style.joyconRight = true;
                    if (controller.type == NpadControllerType::JoyconDual || controller.partnerIndex != -1)
                        style.joyconDual = true;
                } else if (controller.type == NpadControllerType::Handheld) {
                    style.joyconHandheld = true;
                }
                style = NpadStyleSet{.raw = style.raw & styles.raw};

                if (style.raw) {
                    if (style.proController || style.joyconHandheld || style.joyconLeft || style.joyconRight) {
                        device.Connect(controller.type);
                        device.index = static_cast<i8>(&controller - controllers.data());
                        device.partnerIndex = -1;
                        controller.device = &device;
                    } else if (style.joyconDual && orientation == NpadJoyOrientation::Vertical && device.GetAssignment() == NpadJoyAssignment::Dual) {
                        device.Connect(NpadControllerType::JoyconDual);
                        device.index = static_cast<i8>(&controller - controllers.data());
                        device.partnerIndex = controller.partnerIndex;
                        controller.device = &device;
                        controllers.at(static_cast<size_t>(controller.partnerIndex)).device = &device;
                    } else {
                        continue;
                    }
                    break;
                }
            }
        }

        // We do this to prevent triggering the event unless there's a real change in a device's style, which would be caused if we disconnected all controllers then reconnected them
        for (auto &device : npads) {
            if (!ranges::any_of(controllers, [&](auto &controller) { return controller.device == &device; }))
                device.Disconnect();
        }
    }

    void NpadManager::Activate() {
        std::scoped_lock guard{mutex};
        if (!activated) {
            supportedIds = {NpadId::Handheld, NpadId::Player1, NpadId::Player2, NpadId::Player3, NpadId::Player4, NpadId::Player5, NpadId::Player6, NpadId::Player7, NpadId::Player8};
            styles = {.proController = true, .joyconHandheld = true, .joyconDual = true, .joyconLeft = true, .joyconRight = true};
            activated = true;

            Update();
        }
    }

    void NpadManager::Deactivate() {
        std::scoped_lock guard{mutex};
        if (activated) {
            supportedIds = {};
            styles = {};
            activated = false;

            for (auto &npad : npads)
                npad.Disconnect();

            for (auto &controller : controllers)
                controller.device = nullptr;
        }
    }
}
