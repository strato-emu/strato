// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <queue>
#include <android/native_window.h>
#include <kernel/ipc.h>
#include <kernel/types/KEvent.h>
#include <services/nvdrv/devices/nvmap.h>
#include "gpu/texture.h"
#include "gpu/memory_manager.h"
#include "gpu/gpfifo.h"
#include "gpu/syncpoint.h"
#include "gpu/engines/engine.h"
#include "gpu/engines/maxwell_3d.h"

namespace skyline::gpu {
    /**
     * @brief This is used to converge all of the interfaces to the GPU and send the results to a GPU API
     * @note We opted for just supporting a single layer and display as it's what basically all games use and wasting cycles on it is pointless
     */
    class GPU {
      private:
        ANativeWindow *window; //!< The ANativeWindow to render to
        const DeviceState &state; //!< The state of the device
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

        /**
         * @param window The ANativeWindow to render to
         */
        GPU(const DeviceState &state);

        /**
         * @brief The destructor for the GPU class
         */
        ~GPU();

        /**
         * @brief The loop that executes routine GPU functions
         */
        void Loop();
    };
}
