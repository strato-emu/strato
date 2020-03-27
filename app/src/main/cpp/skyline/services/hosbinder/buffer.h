// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <services/common/parcel.h>
#include <services/nvdrv/devices/nvmap.h>
#include <gpu.h>

namespace skyline::service::hosbinder {
    /**
     * @brief This struct holds information about the graphics buffer (https://github.com/reswitched/libtransistor/blob/0f0c36227842c344d163922fc98ee76229e9f0ee/lib/display/graphic_buffer_queue.c#L66)
     */
    struct GbpBuffer {
        u32 magic; //!< The magic of the graphics buffer: 0x47424652
        u32 width; //!< The width of the buffer
        u32 height; //!< The height of the buffer
        u32 stride; //!< The stride of the buffer
        u32 format; //!< The format of the buffer, this corresponds to AHardwareBuffer_Format
        u32 usage; //!< The usage flags for the buffer
        u32 _pad0_;
        u32 index; //!< The index of the buffer
        u32 _pad1_[3];
        u32 nvmapId; //!< The ID of the buffer in regards to /dev/nvmap
        u32 _pad2_[8];
        u32 size; //!< The size of the buffer
        u32 _pad3_[8];
        u32 nvmapHandle; //!< The handle of the buffer in regards to /dev/nvmap
        u32 offset; //!< This is the offset of the pixel data in the GPU Buffer
        u32 _pad4_;
        u32 blockHeightLog2; //!< The log2 of the block height
        u32 _pad5_[58];
    };

    /**
     * @brief The current status of a buffer
     */
    enum class BufferStatus {
        Free, //!< The buffer is free
        Dequeued, //!< The buffer has been dequeued from the display
        Queued, //!< The buffer is queued to be displayed
    };

    /**
     * @brief This holds all relevant objects for a specific buffer
     */
    class Buffer {
      public:
        u32 slot; //!< The slot the buffer is in
        BufferStatus status{BufferStatus::Free}; //!< The status of this buffer
        std::shared_ptr<gpu::PresentationTexture> texture{}; //!< The underlying PresentationTexture of this buffer
        GbpBuffer gbpBuffer; //!< The GbpBuffer object for this buffer

        Buffer(u32 slot, const GbpBuffer &gbpBuffer, const std::shared_ptr<gpu::PresentationTexture> &texture) : slot(slot), gbpBuffer(gbpBuffer), texture(texture) {};
    };
}
