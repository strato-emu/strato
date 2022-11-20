// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::settings {
    /**
     * @brief ISettingsServer or 'set' provides access to user settings
     * @url https://switchbrew.org/wiki/Settings_services#set
     */
    class ISettingsServer : public BaseService {
      public:
        ISettingsServer(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Gets the current system language
         * @url https://switchbrew.org/wiki/Settings_services#GetLanguageCode
         */
        Result GetLanguageCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Reads the available language codes that an application can use (pre 4.0.0)
         */
        Result GetAvailableLanguageCodes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Converts a language code list index to its corresponding language code
         */
        Result MakeLanguageCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the number of available language codes that an application can use (pre 4.0.0)
         */
        Result GetAvailableLanguageCodeCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the user-selected region's code
         */
        Result GetRegionCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Reads the available language codes that an application can use (post 4.0.0)
         */
        Result GetAvailableLanguageCodes2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the number of available language codes that an application can use (post 4.0.0)
         */
        Result GetAvailableLanguageCodeCount2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ISettingsServer, GetLanguageCode),
            SFUNC(0x1, ISettingsServer, GetAvailableLanguageCodes),
            SFUNC(0x2, ISettingsServer, MakeLanguageCode),
            SFUNC(0x3, ISettingsServer, GetAvailableLanguageCodeCount),
            SFUNC(0x4, ISettingsServer, GetRegionCode),
            SFUNC(0x5, ISettingsServer, GetAvailableLanguageCodes2),
            SFUNC(0x6, ISettingsServer, GetAvailableLanguageCodeCount2)
        )
    };
}
