// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2005 The Android Open Source Project
// Copyright © 2019-2020 Ryujinx Team and Contributors

#pragma once

#include <services/common/parcel.h>
#include "android_types.h"
#include "native_window.h"

namespace skyline::gpu {
    class Texture;
}

#define ENUM_CASE(key)   \
    case ENUM_TYPE::key: \
    return #key

#define ENUM_STRING(name, cases)                        \
    constexpr const char *ToString(name value) {        \
        using ENUM_TYPE = name;                         \
        switch (value) {                                \
            cases                                       \
            default:                                    \
                return "Unknown";                       \
        };                                              \
    };

namespace skyline::service::hosbinder {
    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferSlot.h;l=52-91
     */
    enum class BufferState {
        Free,
        Dequeued,
        Queued,
        Acquired,
    };

    ENUM_STRING(BufferState, ENUM_CASE(Free);ENUM_CASE(Dequeued);ENUM_CASE(Queued);ENUM_CASE(Acquired);
    );

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferSlot.h;l=32-138
     */
    struct BufferSlot {
        BufferState state{BufferState::Free};
        u64 frameNumber{}; //!< The amount of frames that have been queued using this slot
        bool wasBufferRequested{}; //!< If GraphicBufferProducer::RequestBuffer has been called with this buffer
        std::shared_ptr<gpu::Texture> texture{};
        std::unique_ptr<GraphicBuffer> graphicBuffer{};
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
     * @brief An endpoint for the GraphicBufferProducer interface, it approximately implements BufferQueueProducer but also implements the functionality of interfaces called into by it such as GraphicBufferConsumer, Gralloc and so on
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/IGraphicBufferProducer.cpp
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueCore.h
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueCore.cpp
     */
    class GraphicBufferProducer {
      public:
        constexpr static u8 MaxSlotCount{16}; //!< The maximum amount of buffer slots that a buffer queue can hold, Android supports 64 but they go unused for applications like games so we've lowered this to 16 (https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueDefs.h;l=29)

      private:
        const DeviceState &state;
        std::mutex mutex; //!< Synchronizes access to the buffer queue
        std::array<BufferSlot, MaxSlotCount> queue;
        u8 activeSlotCount{2}; //!< The amount of slots in the queue that can be used
        u8 hasBufferCount{}; //!< The amount of slots with buffers attached in the queue
        u32 defaultWidth{1}; //!< The assumed width of a buffer if none is supplied in DequeueBuffer
        u32 defaultHeight{1}; //!< The assumed height of a buffer if none is supplied in DequeueBuffer
        AndroidPixelFormat defaultFormat{AndroidPixelFormat::RGBA8888}; //!< The assumed format of a buffer if none is supplied in DequeueBuffer
        NativeWindowApi connectedApi{NativeWindowApi::None}; //!< The API that the producer is currently connected to

        /**
         * @return The amount of buffers which have been queued onto the consumer
         */
        u8 GetPendingBufferCount();

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=67-80;
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=35-40
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=50-73
         */
        AndroidStatus RequestBuffer(i32 slot, GraphicBuffer *&buffer);

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=104-170
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=59-97
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=251-388
         */
        AndroidStatus DequeueBuffer(bool async, u32 width, u32 height, AndroidPixelFormat format, u32 usage, i32 &slot, std::optional<AndroidFence> &fence);

        /**
         * @note Nintendo has added an additional field for swap interval which sets the swap interval of the compositor
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=236-349
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=109-125
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=512-691
         */
        AndroidStatus QueueBuffer(i32 slot, i64 timestamp, bool isAutoTimestamp, AndroidRect crop, NativeWindowScalingMode scalingMode, NativeWindowTransform transform, NativeWindowTransform stickyTransform, bool async, u32 swapInterval, const AndroidFence &fence, u32 &width, u32 &height, NativeWindowTransform &transformHint, u32 &pendingBufferCount);

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=351-359
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=127-132
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=693-720
         */
        void CancelBuffer(i32 slot, const AndroidFence &fence);

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=361-367
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=134-136
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=722-766
         */
        AndroidStatus Query(NativeWindowQuery query, u32 &out);

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=369-405
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=138-148
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=768-831
         */
        AndroidStatus Connect(NativeWindowApi api, bool producerControlledByApp, u32 &width, u32 &height, NativeWindowTransform &transformHint, u32 &pendingBufferCount);

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/IGraphicBufferProducer.h;l=407-426
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/gui/BufferQueueProducer.h;l=150-158
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/BufferQueueProducer.cpp;l=833-890
         */
        AndroidStatus Disconnect(NativeWindowApi api);

        /**
         * @brief Similar to AttachBuffer but the slot is explicitly specified and the producer defaults are set based off it
         * @note This is an HOS-specific addition to GraphicBufferProducer, it exists so that all allocation of buffers is handled by the client to avoid any shared/transfer memory from the client to loan memory for the buffers which would be quite complicated
         */
        AndroidStatus SetPreallocatedBuffer(i32 slot, const GraphicBuffer &graphicBuffer);

      public:
        DisplayId displayId{DisplayId::Null}; //!< The ID of this display
        LayerStatus layerStatus{LayerStatus::Uninitialized}; //!< The status of the single layer the display has

        /**
         * @brief The transactions supported by android.gui.IGraphicBufferProducer
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/IGraphicBufferProducer.cpp;l=35-49
         */
        enum class TransactionCode : u32 {
            RequestBuffer = 1,
            SetBufferCount = 2,
            DequeueBuffer = 3,
            DetachBuffer = 4,
            DetachNextBuffer = 5,
            AttachBuffer = 6,
            QueueBuffer = 7,
            CancelBuffer = 8,
            Query = 9,
            Connect = 10,
            Disconnect = 11,
            SetSidebandStream = 12,
            AllocateBuffers = 13,
            SetPreallocatedBuffer = 14, //!< A transaction specific to HOS, see the implementation for a description of its functionality
        };

        GraphicBufferProducer(const DeviceState &state);

        /**
         * @brief The handler for Binder IPC transactions with IGraphicBufferProducer
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/gui/IGraphicBufferProducer.cpp;l=277-426
         */
        void OnTransact(TransactionCode code, Parcel &in, Parcel &out);

        /**
         * @brief Sets displayId to a specific display type
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

#undef ENUM_CASE
