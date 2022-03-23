// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::aocsrv {
    /**
     * @brief IAddOnContentManager or aoc:u is used by applications to access add-on content information
     * @url https://switchbrew.org/wiki/NS_Services#aoc:u
     */
    class IAddOnContentManager : public BaseService {
      public:
        IAddOnContentManager(const DeviceState &state, ServiceManager &manager);

        Result ListAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x3, IAddOnContentManager, ListAddOnContent)
        )
    };
}
