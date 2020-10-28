// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "gpu/gpfifo.h"
#include "gpu/syncpoint.h"
#include "gpu/engines/maxwell_3d.h"
#include "gpu/presentation_engine.h"

namespace skyline::gpu {
    /**
     * @brief A common interfaces to the GPU where all objects relevant to it are present
     */
    class GPU {
      private:
        const DeviceState &state;

      public:
        PresentationEngine presentation;
        vmm::MemoryManager memoryManager;
        std::shared_ptr<engine::Engine> fermi2D;
        std::shared_ptr<engine::Maxwell3D> maxwell3D;
        std::shared_ptr<engine::Engine> maxwellCompute;
        std::shared_ptr<engine::Engine> maxwellDma;
        std::shared_ptr<engine::Engine> keplerMemory;
        gpfifo::GPFIFO gpfifo;
        std::array<Syncpoint, constant::MaxHwSyncpointCount> syncpoints{};

        inline GPU(const DeviceState &state) : state(state), presentation(state), memoryManager(state), gpfifo(state), fermi2D(std::make_shared<engine::Engine>(state)), keplerMemory(std::make_shared<engine::Engine>(state)), maxwell3D(std::make_shared<engine::Maxwell3D>(state)), maxwellCompute(std::make_shared<engine::Engine>(state)), maxwellDma(std::make_shared<engine::Engine>(state)) {}
    };
}
