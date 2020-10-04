// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "input.h"

namespace skyline::input {
    Input::Input(const DeviceState &state) : state(state), kHid(std::make_shared<kernel::type::KSharedMemory>(state, sizeof(HidSharedMemory))), hid(reinterpret_cast<HidSharedMemory *>(kHid->kernel.ptr)), npad(state, hid), touch(state, hid) {}
}
