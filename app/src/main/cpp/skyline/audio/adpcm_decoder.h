// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::audio {
    /**
     * @brief The AdpcmDecoder class handles decoding single channel ADPCM (Adaptive Differential Pulse-Code Modulation) data
     */
    class AdpcmDecoder {
      private:
        union FrameHeader {
            u8 raw;

            struct {
                u8 scale : 4; //!< The scale factor for this frame
                u8 coefficientIndex : 3;
                u8 _pad_ : 1;
            };
        };
        static_assert(sizeof(FrameHeader) == 0x1);

        std::array<i32, 2> history{}; //!< The previous samples for decoding the ADPCM stream
        std::vector<std::array<i16, 2>> coefficients; //!< The coefficients for decoding the ADPCM stream

      public:
        AdpcmDecoder(std::vector<std::array<i16, 2>> coefficients);

        /**
         * @brief Decodes a buffer of ADPCM data into I16 PCM
         */
        std::vector<i16> Decode(span <u8> adpcmData);
    };
}
