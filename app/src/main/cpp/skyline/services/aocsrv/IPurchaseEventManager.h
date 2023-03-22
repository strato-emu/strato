// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>

namespace skyline::service::aocsrv {
    /**
     * @url https://switchbrew.org/wiki/NS_Services#IPurchaseEventManager
     */
    class IPurchaseEventManager : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> purchasedEvent;

      public:
        IPurchaseEventManager(const DeviceState &state, ServiceManager &manager);

        Result SetDefaultDeliveryTarget(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetPurchasedEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result PopPurchasedProductInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IPurchaseEventManager, SetDefaultDeliveryTarget),
            SFUNC(0x2, IPurchaseEventManager, GetPurchasedEventReadableHandle),
            SFUNC(0x3, IPurchaseEventManager, PopPurchasedProductInfo)
        )
    };
}
