// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::settings {
    /**
     * @brief ISettingsServer or 'set' provides access to user settings (https://switchbrew.org/wiki/Settings_services#set)
     */
    class ISettingsServer : public BaseService {
      public:
        ISettingsServer(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This reads the available language codes that an application can use
         */
        void GetAvailableLanguageCodes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
