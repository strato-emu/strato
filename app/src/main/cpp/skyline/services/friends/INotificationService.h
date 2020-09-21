// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::friends {
    /**
     * @brief INotificationService is used by applications to receive notifications (https://switchbrew.org/wiki/Friend_services#INotificationService)
     */
    class INotificationService : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> notificationEvent; //!< This KEvent is triggered on new notifications

      public:
        INotificationService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns a handle to the notification event
         */
        Result GetEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, INotificationService, GetEvent)
        )
    };
}
