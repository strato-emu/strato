// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::bcat {

    class IDeliveryCacheProgressService : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> systemEvent;
      public:
        IDeliveryCacheProgressService(const DeviceState &state, ServiceManager &manager);

        Result GetEvent (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetImpl (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
        SFUNC(0x0, IDeliveryCacheProgressService, GetEvent),
        SFUNC(0x1, IDeliveryCacheProgressService, GetImpl),
        )
    };
}
