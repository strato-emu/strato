#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <services/common/parcel.h>
#include "buffer.h"
#include "display.h"

namespace skyline::service::hosbinder {
    /**
     * @brief This enumerates the functions called by TransactParcel for android.gui.IGraphicBufferProducer
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
        SetPreallocatedBuffer = 14, //!< No source on this but it's used to set a existing buffer according to libtransistor and libNX
    };

    /**
     * @brief This represents conditions for the completion of an asynchronous graphics operation
     */
    struct Fence {
        u32 syncptId; //!< The ID of the syncpoint
        u32 syncptValue; //!< The value of the syncpoint
    };

    /**
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is responsible for writing buffers to the display
     */
    class IHOSBinderDriver : public BaseService {
      private:
        /**
         * @brief This is the structure of the parcel used for TransactionCode::Connect
         */
        struct ConnectParcel {
            u32 width; //!< The width of the display
            u32 height; //!< The height of the display
            u32 transformHint; //!< A hint of the transformation of the display
            u32 pendingBuffers; //!< The number of pending buffers
            u32 status; //!< The status of the buffer queue
        };

        std::unordered_map<u32, std::shared_ptr<Buffer>> queue; //!< A vector of shared pointers to all the queued buffers
        /**
         * @brief This the GbpBuffer struct of the specified buffer
         */
        void RequestBuffer(Parcel &in, Parcel &out);

        /**
         * @brief This returns the slot of a free buffer
         */
        void DequeueBuffer(Parcel &in, Parcel &out);

        /**
         * @brief This queues a buffer to be displayed
         */
        void QueueBuffer(Parcel &in, Parcel &out);

        /**
         * @brief This removes a previously queued buffer
         */
        void CancelBuffer(Parcel &parcel);

        /**
         * @brief This adds a pre-existing buffer to the queue
         */
        void SetPreallocatedBuffer(Parcel &parcel);

        /**
         * @brief This frees a buffer which is currently queued
         * @param slotNo The slot of the buffer
         */
        inline void FreeBuffer(u32 slotNo) {
            queue.at(slotNo)->status = BufferStatus::Free;
        }

      public:
        DisplayId displayId{DisplayId::Null}; //!< The ID of this display
        LayerStatus layerStatus{LayerStatus::Uninitialized}; //!< This is the status of the single layer the display has

        IHOSBinderDriver(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This emulates the transaction of parcels between a IGraphicBufferProducer and the application (https://switchbrew.org/wiki/Nvnflinger_services#TransactParcel)
         */
        void TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This adjusts the reference counts to the underlying binder, it is stubbed as we aren't using the real symbols (https://switchbrew.org/wiki/Nvnflinger_services#AdjustRefcount)
         */
        void AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This adjusts the reference counts to the underlying binder, it is stubbed as we aren't using the real symbols (https://switchbrew.org/wiki/Nvnflinger_services#GetNativeHandle)
         */
        void GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets displayId to a specific display type
         * @param name The name of the display
         * @note displayId has to be DisplayId::Null or this will throw an exception
         */
        void SetDisplay(const std::string &name);

        /**
         * @brief This closes the display by setting displayId to DisplayId::Null
         */
        void CloseDisplay();
    };
}
