// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::timesrv::result {
    constexpr Result PermissionDenied(116, 1);
    constexpr Result ClockSourceIdMismatch(116, 102);
    constexpr Result ClockUninitialized(116, 103);
    constexpr Result InvalidComparison(116, 200);
    constexpr Result CompareOverflow(116, 201);
    constexpr Result RuleConversionFailed(116, 903);
    constexpr Result Unimplemented(116, 990);
}