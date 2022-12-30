// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::aocsrv {
    /**
     * @brief IAddOnContentManager or aoc:u is used by applications to access add-on content information
     * @url https://switchbrew.org/wiki/NS_Services#aoc:u
     */
    class IAddOnContentManager : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> addOnContentListChangedEvent; //!< This KEvent is triggered when the add-on content list changes

      public:
        IAddOnContentManager(const DeviceState &state, ServiceManager &manager);

        Result CountAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result ListAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetAddOnContentListChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetAddOnContentListChangedEventWithProcessId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result CheckAddOnContentMountStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result CreateEcPurchasedEventManager(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x2, IAddOnContentManager, CountAddOnContent),
            SFUNC(0x3, IAddOnContentManager, ListAddOnContent),
            SFUNC(0x8, IAddOnContentManager, GetAddOnContentListChangedEvent),
            SFUNC(0xA, IAddOnContentManager, GetAddOnContentListChangedEventWithProcessId),
            SFUNC(0x32, IAddOnContentManager, CheckAddOnContentMountStatus),
            SFUNC(0x64, IAddOnContentManager, CreateEcPurchasedEventManager)
        )
    };
}
