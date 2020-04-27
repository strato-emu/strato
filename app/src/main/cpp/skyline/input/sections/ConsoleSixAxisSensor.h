// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::input {
    /**
     * @brief The structure of the ConsoleSixAxisSensor section (https://switchbrew.org/wiki/HID_Shared_Memory#ConsoleSixAxisSensor)
     * @note This is seemingly used to calibrate the gyroscope bias values over time
     */
    struct ConsoleSixAxisSensorSection {
        u64 timestamp; //!< The timestamp in samples
        bool resting; //!< If the sensors are at rest or not (The calibration is performed when the devices are at rest)
        u8 _pad0_[0x3];
        u32 verticalizationError; //!< The error in verticalization ?
        u32 gyroBias[3]; //!< The gyroscope's sensor bias in all axis
        u32 _pad1_;
    };
    static_assert(sizeof(ConsoleSixAxisSensorSection) == 0x20);
}
