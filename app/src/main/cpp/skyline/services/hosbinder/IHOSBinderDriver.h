// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::hosbinder {
    class GraphicBufferProducer;

    /**
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is responsible for writing buffers to the display
     */
    class IHOSBinderDriver : public BaseService {
      private:
        std::shared_ptr<GraphicBufferProducer> producer;

      public:
        IHOSBinderDriver(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This emulates the transaction of parcels between a IGraphicBufferProducer and the application (https://switchbrew.org/wiki/Nvnflinger_services#TransactParcel)
         */
        Result TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This adjusts the reference counts to the underlying binder, it is stubbed as we aren't using the real symbols (https://switchbrew.org/wiki/Nvnflinger_services#AdjustRefcount)
         */
        Result AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This adjusts the reference counts to the underlying binder, it is stubbed as we aren't using the real symbols (https://switchbrew.org/wiki/Nvnflinger_services#GetNativeHandle)
         */
        Result GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
