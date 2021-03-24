// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

#define U32_OFFSET(regs, field) (offsetof(regs, field) / sizeof(u32))

namespace skyline::soc::gm20b {
    enum class EngineID {
        Fermi2D = 0x902D,
        KeplerMemory = 0xA140,
        Maxwell3D = 0xB197,
        MaxwellCompute = 0xB1C0,
        MaxwellDma = 0xB0B5,
    };

    /**
     * @brief The parameters of a GPU engine method call
     */
    struct MethodParams {
        u16 method;
        u32 argument;
        u32 subChannel;
        bool lastCall; //!< If this is the last call in the pushbuffer entry to this specific macro
    };

    namespace engine {
        /**
         * @brief The Engine class provides an interface that can be used to communicate with the GPU's internal engines
         */
        class Engine {
          protected:
            const DeviceState &state;

          public:
            Engine(const DeviceState &state) : state(state) {}

            virtual ~Engine() = default;

            /**
             * @brief Calls an engine method with the given parameters
             */
            virtual void CallMethod(MethodParams params) {
                state.logger->Warn("Called method in unimplemented engine: 0x{:X} args: 0x{:X}", params.method, params.argument);
            };
        };
    }
}
