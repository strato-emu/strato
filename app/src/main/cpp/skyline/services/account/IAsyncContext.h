// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::account {
    /**
    * @url https://switchbrew.org/wiki/Account_services#IAsyncContext
    */
    class IAsyncContext : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> systemEvent;

      public:
        IAsyncContext(const DeviceState &state, ServiceManager &manager);

        Result GetSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result Cancel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result HasDone(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IAsyncContext, GetSystemEvent),
            SFUNC(0x1, IAsyncContext, Cancel),
            SFUNC(0x2, IAsyncContext, HasDone),
            SFUNC(0x3, IAsyncContext, GetResult)
        )
    };
}
