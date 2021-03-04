// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::hosbinder {
    class GraphicBufferProducer;

    /**
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is a translation layer between Android Binder IPC and HOS IPC to communicate with the Android display stack
     */
    class IHOSBinderDriver : public BaseService {
      private:
        std::shared_ptr<GraphicBufferProducer> producer;

      public:
        IHOSBinderDriver(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Emulates the transaction of parcels between a IGraphicBufferProducer and the application
         * @url https://switchbrew.org/wiki/Nvnflinger_services#TransactParcel
         */
        Result TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Adjusts the reference counts to the underlying binder, it's stubbed as we aren't using the real symbols
         * @url https://switchbrew.org/wiki/Nvnflinger_services#AdjustRefcount
         */
        Result AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Adjusts the reference counts to the underlying binder, it's stubbed as we aren't using the real symbols
         * @url https://switchbrew.org/wiki/Nvnflinger_services#GetNativeHandle
         */
        Result GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IHOSBinderDriver, TransactParcel),
            SFUNC(0x1, IHOSBinderDriver, AdjustRefcount),
            SFUNC(0x2, IHOSBinderDriver, GetNativeHandle),
            SFUNC(0x3, IHOSBinderDriver, TransactParcel)
        )
    };
}
