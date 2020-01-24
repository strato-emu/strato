#pragma once

#include <common.h>

namespace skyline::audio {
    /**
     * @brief The Resampler class handles resampling audio PCM data
     */
    class Resampler {
      private:
        uint fraction{}; //!< The fractional value used for storing the resamplers last frame

      public:
        /**
         * @brief Resamples the given sample buffer by the given ratio
         * @param inputBuffer A buffer containing PCM sample data
         * @param ratio The conversion ratio needed
         * @param channelCount The amount of channels the buffer contains
         */
        std::vector<i16> ResampleBuffer(std::vector<i16> &inputBuffer, double ratio, int channelCount);
    };
}
