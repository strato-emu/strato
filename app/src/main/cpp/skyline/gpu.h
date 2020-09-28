// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <android/native_window.h>
#include "services/nvdrv/devices/nvmap.h"
#include "gpu/gpfifo.h"
#include "gpu/syncpoint.h"
#include "gpu/engines/maxwell_3d.h"

namespace skyline::gpu {
    /**
     * @brief A common interfaces to the GPU where all objects relevant to it are present
     */
    class GPU {
      private:
        ANativeWindow *window; //!< The ANativeWindow that is presented to
        const DeviceState &state;
        bool surfaceUpdate{}; //!< If the surface needs to be updated
        u64 frameTimestamp{}; //!< The timestamp of the last frame being shown

      public:
        std::queue<std::shared_ptr<PresentationTexture>> presentationQueue; //!< A queue of all the PresentationTextures to be posted to the display
        texture::Dimensions resolution{}; //!< The resolution of the surface
        i32 format{}; //!< The format of the display window
        std::shared_ptr<kernel::type::KEvent> vsyncEvent; //!< This KEvent is triggered every time a frame is drawn
        std::shared_ptr<kernel::type::KEvent> bufferEvent; //!< This KEvent is triggered every time a buffer is freed
        vmm::MemoryManager memoryManager; //!< The GPU Virtual Memory Manager
        std::shared_ptr<engine::Engine> fermi2D;
        std::shared_ptr<engine::Maxwell3D> maxwell3D;
        std::shared_ptr<engine::Engine> maxwellCompute;
        std::shared_ptr<engine::Engine> maxwellDma;
        std::shared_ptr<engine::Engine> keplerMemory;
        gpfifo::GPFIFO gpfifo;
        std::array<Syncpoint, constant::MaxHwSyncpointCount> syncpoints{};

        GPU(const DeviceState &state);

        ~GPU();

        void Loop();
    };
}
