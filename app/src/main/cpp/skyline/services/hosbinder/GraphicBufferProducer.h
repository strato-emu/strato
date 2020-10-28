// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/common/parcel.h>

namespace skyline::gpu {
    class Texture;
}

namespace skyline::service::hosbinder {
    /**
     * @brief A descriptor for the surfaceflinger graphics buffer
     * @url https://github.com/reswitched/libtransistor/blob/0f0c36227842c344d163922fc98ee76229e9f0ee/lib/display/graphic_buffer_queue.c#L66
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
        u32 offset; //!< The offset of the pixel data in the GPU Buffer
        u32 _pad4_;
        u32 blockHeightLog2; //!< The log2 of the block height
        u32 _pad5_[58];
    };

    enum class BufferStatus {
        Free, //!< The buffer is free
        Dequeued, //!< The buffer has been dequeued from the display
        Queued, //!< The buffer is queued to be displayed
    };

    /**
     * @brief A wrapper over GbpBuffer which contains additional state that we track for a buffer
     */
    class Buffer {
      public:
        BufferStatus status{BufferStatus::Free};
        std::shared_ptr<gpu::Texture> texture;
        GbpBuffer gbpBuffer;

        Buffer(const GbpBuffer &gbpBuffer, const std::shared_ptr<gpu::Texture> &texture);
    };

    /**
     * @brief An enumeration of all the possible display IDs
     * @url https://switchbrew.org/wiki/Display_services#DisplayName
     */
    enum class DisplayId : u64 {
        Default, //!< Refers to the default display used by most applications
        External, //!< Refers to an external display
        Edid, //!< Refers to an external display with EDID capabilities
        Internal, //!< Refers to the the internal display
        Null, //!< Refers to the null display which is used for discarding data
    };

    enum class LayerStatus {
        Uninitialized, //!< The layer hasn't been initialized
        Stray, //!< The layer has been initialized as a stray layer
        Managed, //!< The layer has been initialized as a managed layer
    };

    /**
     * @brief IGraphicBufferProducer is responsible for presenting buffers to the display as well as compositing and frame pacing
     * @url https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp
     */
    class GraphicBufferProducer {
      private:
        const DeviceState &state;
        std::unordered_map<u32, std::shared_ptr<Buffer>> queue; //!< A vector of shared pointers to all the queued buffers

        /**
         * @brief Request for the GbpBuffer of a buffer
         */
        void RequestBuffer(Parcel &in, Parcel &out);

        /**
         * @brief Try to dequeue a free graphics buffer that has been consumed
         */
        void DequeueBuffer(Parcel &in, Parcel &out);

        /**
         * @brief Queue a buffer to be presented
         */
        void QueueBuffer(Parcel &in, Parcel &out);

        /**
         * @brief Remove a previously queued buffer
         */
        void CancelBuffer(Parcel &in);

        /**
         * @brief Query a few attributes of the graphic buffers
         */
        void Connect(Parcel &out);

        /**
         * @brief Attach a GPU buffer to a graphics buffer
         */
        void SetPreallocatedBuffer(Parcel &in);

      public:
        DisplayId displayId{DisplayId::Null}; //!< The ID of this display
        LayerStatus layerStatus{LayerStatus::Uninitialized}; //!< The status of the single layer the display has

        /**
         * @brief The functions called by TransactParcel for android.gui.IGraphicBufferProducer
         * @refitem https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#35
         */
        enum class TransactionCode : u32 {
            RequestBuffer = 1, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#281
            SetBufferCount = 2, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#293
            DequeueBuffer = 3, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#300
            DetachBuffer = 4, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#318
            DetachNextBuffer = 5, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#325
            AttachBuffer = 6, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#343
            QueueBuffer = 7, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#353
            CancelBuffer = 8, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#364
            Query = 9, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#372
            Connect = 10, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#381
            Disconnect = 11, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#396
            SetSidebandStream = 12, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#403
            AllocateBuffers = 13, //!< https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#413
            SetPreallocatedBuffer = 14, //!< No source on this but it's used to set a existing buffer according to libtransistor and libnx
        };

        GraphicBufferProducer(const DeviceState &state);

        /**
         * @brief The handler for Binder IPC transactions with IGraphicBufferProducer
         * @url https://android.googlesource.com/platform/frameworks/native/+/8dc5539/libs/gui/IGraphicBufferProducer.cpp#277
         */
        void OnTransact(TransactionCode code, Parcel &in, Parcel &out);

        /**
         * @brief Sets displayId to a specific display type
         * @param name The name of the display
         * @note displayId has to be DisplayId::Null or this will throw an exception
         */
        void SetDisplay(const std::string &name);

        /**
         * @brief Closes the display by setting displayId to DisplayId::Null
         */
        void CloseDisplay();
    };

    extern std::weak_ptr<GraphicBufferProducer> producer; //!< A globally shared instance of the GraphicsBufferProducer
}
