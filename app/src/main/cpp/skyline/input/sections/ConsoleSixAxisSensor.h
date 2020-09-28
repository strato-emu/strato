// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @url https://switchbrew.org/wiki/HID_Shared_Memory#ConsoleSixAxisSensor
     * @note Referred to as SevenSixAxisSensor in HID Services
     * @note This is seemingly used to calibrate the gyroscope bias values over time
     */
    struct ConsoleSixAxisSensorSection {
        u64 timestamp; //!< The timestamp in samples
        bool resting; //!< If the sensors are at rest or not (Calibration is performed when the sensors are at rest)
        u8 _pad0_[0x3];
        u32 verticalizationError; //!< The error in sensor fusion
        std::array<u32, 3> gyroBias; //!< The gyroscope's sensor bias in all axis
        u32 _pad1_;
    };
    static_assert(sizeof(ConsoleSixAxisSensorSection) == 0x20);
}
