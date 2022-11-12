// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/command_executor.h>
#include "macro/macro_state.h"
#include "engines/engine.h"
#include "engines/maxwell_3d.h"
#include "engines/fermi_2d.h"
#include "engines/maxwell_dma.h"
#include "engines/kepler_compute.h"
#include "engines/inline2memory.h"
#include "gpfifo.h"

namespace skyline::soc::gm20b {
    struct AddressSpaceContext;

    /**
     * @brief The GPU block in the X1, it contains all GPU engines required for accelerating graphics operations
     * @note We omit parts of components related to external access such as the grhost, all accesses to the external components are done directly
     */
    struct ChannelContext {
        std::shared_ptr<AddressSpaceContext> asCtx;
        gpu::interconnect::CommandExecutor executor;
        MacroState macroState;
        engine::maxwell3d::Maxwell3D maxwell3D;
        engine::fermi2d::Fermi2D fermi2D;
        engine::MaxwellDma maxwellDma;
        engine::KeplerCompute keplerCompute;
        engine::Inline2Memory inline2Memory;
        ChannelGpfifo gpfifo;
        std::mutex &globalChannelLock;
        size_t channelSequenceNumber{};

        ChannelContext(const DeviceState &state, std::shared_ptr<AddressSpaceContext> asCtx, size_t numEntries);

        void Lock() {
            globalChannelLock.lock();
            executor.LockPreserve();
        }

        void Unlock() {
            executor.UnlockPreserve();
            globalChannelLock.unlock();
        }
    };
}
