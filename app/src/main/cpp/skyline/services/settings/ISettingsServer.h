// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service {
    namespace constant {
        constexpr size_t OldLanguageCodeListSize{15}; //!< The size of the pre 4.0.0 language code list
        constexpr size_t NewLanguageCodeListSize{17}; //!< The size of the post 4.0.0 language code list
    }

    namespace settings {
        /**
         * @brief ISettingsServer or 'set' provides access to user settings (https://switchbrew.org/wiki/Settings_services#set)
         */
        class ISettingsServer : public BaseService {
          public:
            ISettingsServer(const DeviceState &state, ServiceManager &manager);

            /**
             * @brief This reads the available language codes that an application can use (pre 4.0.0)
             */
            Result GetAvailableLanguageCodes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief This converts a language code list index to it's corresponding language code
             */
            Result MakeLanguageCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            /**
             * @brief This reads the available language codes that an application can use (post 4.0.0)
             */
            Result GetAvailableLanguageCodes2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

            SERVICE_DECL(
                SFUNC(0x1, ISettingsServer, GetAvailableLanguageCodes),
                SFUNC(0x2, ISettingsServer, MakeLanguageCode),
                SFUNC(0x5, ISettingsServer, GetAvailableLanguageCodes2)
            )
        };
    }
}
