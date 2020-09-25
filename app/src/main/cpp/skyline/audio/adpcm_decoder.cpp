// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <array>
#include <common.h>
#include "common.h"
#include "adpcm_decoder.h"

namespace skyline::audio {
    AdpcmDecoder::AdpcmDecoder(const std::vector<std::array<i16, 2>> &coefficients) : coefficients(coefficients) {}

    std::vector<i16> AdpcmDecoder::Decode(span<u8> adpcmData) {
        constexpr size_t BytesPerFrame{0x8};
        constexpr size_t SamplesPerFrame{0xE};

        size_t remainingSamples{(adpcmData.size() / BytesPerFrame) * SamplesPerFrame};

        std::vector<i16> output;
        output.reserve(remainingSamples);

        size_t inputOffset{};

        while (inputOffset < adpcmData.size()) {
            FrameHeader header{adpcmData[inputOffset++]};

            size_t frameSamples{std::min(SamplesPerFrame, remainingSamples)};

            i32 ctx{};

            for (size_t index = 0; index < frameSamples; index++) {
                i32 sample{};

                if (index & 1) {
                    sample = (ctx << 28) >> 28;
                } else {
                    ctx = adpcmData[inputOffset++];
                    sample = (ctx << 24) >> 28;
                }

                i32 prediction = history[0] * coefficients[header.coefficientIndex][0] + history[1] * coefficients[header.coefficientIndex][1];
                sample = (sample * (0x800 << header.scale) + prediction + 0x400) >> 11;

                auto saturated = audio::Saturate<i16, i32>(sample);
                output.push_back(saturated);
                history[1] = history[0];
                history[0] = saturated;
            }

            remainingSamples -= frameSamples;
        }

        return output;
    }
}
