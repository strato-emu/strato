// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service {
    enum class PosixResult : i32 {
        Success = 0,
        NotPermitted = 1, // EPERM
        TryAgain = 11, // EAGAIN
        Busy = 16, // EBUSY
        InvalidArgument = 22, // EINVAL
        InappropriateIoctlForDevice = 25, // ENOTTY
        FunctionNotImplemented = 38, // ENOSYS
        NotSupported = 95, // EOPNOTSUPP, ENOTSUP
        TimedOut = 110, // ETIMEDOUT
    };

    template<typename ValueType>
    using PosixResultValue = ResultValue<ValueType, PosixResult>;
}
