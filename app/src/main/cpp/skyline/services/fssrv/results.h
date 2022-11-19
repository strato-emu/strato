// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::fssrv::result {
    constexpr Result PathDoesNotExist(2, 1);
    constexpr Result NoRomFsAvailable(2, 1001);
    constexpr Result EntityNotFound(2, 1002);
    constexpr Result UnexpectedFailure(2, 5000);
    constexpr Result InvalidArgument(2, 6001);
    constexpr Result InvalidOffset(2, 6061);
    constexpr Result InvalidSize(2, 6062);
}