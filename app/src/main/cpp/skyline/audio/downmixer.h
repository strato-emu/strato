// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "common.h"

namespace skyline::audio {
    /**
     * @brief Format of a 5.1 channel audio sample
     */
    struct Surround51Sample {
        i16 frontLeft;
        i16 frontRight;
        i16 centre;
        i16 lowFrequency;
        i16 backLeft;
        i16 backRight;
    };

    /**
     * @brief Format of a stereo audio sample
     */
    struct StereoSample {
        i16 left;
        i16 right;
    };

    /**
     * @brief Downmixes a buffer of 5.1 surround audio to stereo
     */
    inline std::vector<StereoSample> DownMix(span<Surround51Sample> surroundSamples) {
        constexpr i16 FixedPointMultiplier{1000}; //!< Avoids using floating point maths
        constexpr i16 Attenuation3Db{707}; //! 10^(-3/20)
        constexpr i16 Attenuation6Db{501}; //! 10^(-6/20)
        constexpr i16 Attenuation12Db{251}; //! 10^(-6/20)

        auto downmixChannel{[](i32 front, i32 centre, i32 lowFrequency, i32 back) {
            return static_cast<i16>(front +
                                    (centre * Attenuation3Db +
                                     lowFrequency * Attenuation12Db +
                                     back * Attenuation6Db) / FixedPointMultiplier);
        }};

        std::vector<StereoSample> stereoSamples(surroundSamples.size());
        for (size_t i{}; i < surroundSamples.size(); i++) {
            auto &surroundSample = surroundSamples[i];
            auto &stereoSample = stereoSamples[i];

            stereoSample.left = downmixChannel(surroundSample.frontLeft, surroundSample.centre, surroundSample.lowFrequency, surroundSample.backLeft);
            stereoSample.right = downmixChannel(surroundSample.frontRight, surroundSample.centre, surroundSample.lowFrequency, surroundSample.backRight);
        }

        return stereoSamples;
    }
}
