// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ntc {
    namespace result {
        constexpr Result NetworkTimeNotAvailable{116, 1000};
    }

    /**
    * @url https://switchbrew.org/wiki/NIM_services#IEnsureNetworkClockAvailabilityService
    */
    class IEnsureNetworkClockAvailabilityService : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> finishNotificationEvent;

      public:
        IEnsureNetworkClockAvailabilityService(const DeviceState &state, ServiceManager &manager);

        Result StartTask(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IEnsureNetworkClockAvailabilityService, StartTask)
        )
    };
}
