// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/command_executor.h>
#include "engines/engine.h"
#include "gpfifo.h"

namespace skyline::soc::gm20b {
    namespace engine::maxwell3d {
        class Maxwell3D;
    }

    struct AddressSpaceContext;

    /**
     * @brief The GPU block in the X1, it contains all GPU engines required for accelerating graphics operations
     * @note We omit parts of components related to external access such as the grhost, all accesses to the external components are done directly
     */
    struct ChannelContext {
        std::shared_ptr<AddressSpaceContext> asCtx;
        gpu::interconnect::CommandExecutor executor;
        engine::Engine fermi2D;
        std::unique_ptr<engine::maxwell3d::Maxwell3D> maxwell3D; //!< TODO: fix this once graphics context is moved into a cpp file
        engine::Engine maxwellCompute;
        engine::Engine maxwellDma;
        engine::Engine keplerMemory;
        ChannelGpfifo gpfifo;

        ChannelContext(const DeviceState &state, std::shared_ptr<AddressSpaceContext> asCtx, size_t numEntries);
    };
}
