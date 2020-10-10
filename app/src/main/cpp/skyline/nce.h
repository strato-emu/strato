// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"
#include <sys/wait.h>

namespace skyline::nce {
    /**
     * @brief The NCE (Native Code Execution) class is responsible for managing the state of catching instructions and directly controlling processes/threads
     */
    class NCE {
      private:
        DeviceState &state;

        static void SvcHandler(u16 svc, ThreadContext* ctx);

      public:
        static void SignalHandler(int signal, siginfo *info, void *context);

        NCE(DeviceState &state);

        void Execute();

        /**
         * @brief Generates a patch section for the supplied code
         * @param baseAddress The address at which the code is mapped
         * @param patchBase The offset of the patch section from the base address
         */
        std::vector<u32> PatchCode(std::vector<u8> &code, u64 baseAddress, i64 patchBase);
    };
}
