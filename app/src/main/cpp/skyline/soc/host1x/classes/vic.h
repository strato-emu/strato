// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::soc::host1x {
    /**
     * @brief The VIC Host1x class implements hardware accelerated image operations
     */
    class VicClass {
      private:
        std::function<void()> opDoneCallback;

      public:
        VicClass(std::function<void()> opDoneCallback);

        void CallMethod(u32 method, u32 argument);
    };
}
