#pragma once

#include "services/base_service.h"
#include "services/serviceman.h"
#include <gpu/parcel.h>

namespace skyline::kernel::service::nvnflinger {
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
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is responsible for writing buffers to the display
     */
    class dispdrv : public BaseService {
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

      public:
        dispdrv(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This emulates the transaction of parcels between a IGraphicBufferProducer and the application (https://switchbrew.org/wiki/Nvnflinger_services#TransactParcel)
         */
        void TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This adjusts the reference counts to the underlying binder, it is stubbed as we aren't using the real symbols (https://switchbrew.org/wiki/Nvnflinger_services#GetNativeHandle)
         */
        void GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This adjusts the reference counts to the underlying binder, it is stubbed as we aren't using the real symbols (https://switchbrew.org/wiki/Nvnflinger_services#AdjustRefcount)
         */
        void AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
