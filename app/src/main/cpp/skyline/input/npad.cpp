// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "npad.h"

namespace skyline::input {

    NpadManager::NpadManager(const DeviceState &state, input::HidSharedMemory *hid) : state(state), npads
        {NpadDevice{hid->npad[0], NpadId::Player1}, {hid->npad[1], NpadId::Player2},
         {hid->npad[2], NpadId::Player3}, {hid->npad[3], NpadId::Player4},
         {hid->npad[4], NpadId::Player5}, {hid->npad[5], NpadId::Player6},
         {hid->npad[6], NpadId::Player7}, {hid->npad[7], NpadId::Player8},
         {hid->npad[8], NpadId::Unknown}, {hid->npad[9], NpadId::Handheld},
        } {}

    void NpadManager::Activate() {
        if (styles.raw == 0) {
            styles.proController = true;
            styles.joyconHandheld = true;
        }

        at(NpadId::Player1).Connect(state.settings->GetBool("operation_mode") ? NpadControllerType::Handheld : NpadControllerType::ProController);
    }

    void NpadManager::Deactivate() {
        styles.raw = 0;

        for (auto &npad : npads)
            npad.Disconnect();
    }
}
