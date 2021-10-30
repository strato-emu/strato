// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

#define U32_OFFSET(regs, field) (offsetof(regs, field) / sizeof(u32))

namespace skyline::soc::gm20b {
    namespace engine {
        /**
         * @brief The Engine class provides an interface that can be used to communicate with the GPU's internal engines
         */
        class Engine {
          protected:
            const DeviceState &state;

          public:
            Engine(const DeviceState &state) : state(state) {}

            /**
             * @brief Calls an engine method with the given parameters
             */
            void CallMethod(u32 method, u32 argument, bool lastCall) {
                state.logger->Warn("Called method in unimplemented engine: 0x{:X} args: 0x{:X}", method, argument);
            };
        };
    }
}
