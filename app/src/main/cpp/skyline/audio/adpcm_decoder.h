// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <span>
#include <common.h>

namespace skyline::audio {
    /**
     * @brief The AdpcmDecoder class handles decoding single channel adaptive differential PCM data
     */
    class AdpcmDecoder {
      private:
        /**
         * @brief This struct holds a single ADPCM frame header
         */
        union FrameHeader {
            u8 raw;

            struct {
                u8 scale : 4; //!< The scale factor for this frame
                u8 coefficientIndex : 3;
                u8 _pad_ :1;
            };
        };
        static_assert(sizeof(FrameHeader) == 0x1);

        std::array<i32, 2> history{}; //!< This contains the history for decoding the ADPCM stream
        std::vector<std::array<i16, 2>> coefficients; //!< This contains the coefficients for decoding the ADPCM stream

      public:
        AdpcmDecoder(const std::vector<std::array<i16, 2>> &coefficients);

        /**
         * @brief This decodes a buffer of ADPCM data into I16 PCM
         * @param adpcmData A buffer containing the raw ADPCM data
         * @return A buffer containing decoded single channel I16 PCM data
         */
        std::vector<i16> Decode(span<u8> adpcmData);
    };
}
