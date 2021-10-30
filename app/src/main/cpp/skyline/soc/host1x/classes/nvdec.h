// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::soc::host1x {
    /**
     * @brief The NVDEC Host1x class implements hardware accelerated video decoding for the VP9/VP8/H264/VC1 codecs
     */
    class NvDecClass {
      private:
        const DeviceState &state;
        std::function<void()> opDoneCallback;

      public:
        NvDecClass(const DeviceState &state, std::function<void()> opDoneCallback);

        void CallMethod(u32 method, u32 argument);
    };
}
