// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2019-2022 Ryujinx Team and Contributors

#include "software_keyboard_config.h"

namespace skyline::applet::swkbd {
    KeyboardConfigVB::KeyboardConfigVB() = default;

    KeyboardConfigVB::KeyboardConfigVB(const KeyboardConfigV7 &v7config) : commonConfig{v7config.commonConfig}, separateTextPos{v7config.separateTextPos} {}

    KeyboardConfigVB::KeyboardConfigVB(const KeyboardConfigV0 &v0config) : commonConfig{v0config.commonConfig} {}
}
