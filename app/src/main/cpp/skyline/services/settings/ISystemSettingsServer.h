// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::settings {
    /**
     * @brief ISystemSettingsServer or set:sys service provides access to system settings
     */
    class ISystemSettingsServer : public BaseService {
      private:
        /**
         * @brief Encapsulates the system version, this is sent to the application in GetFirmwareVersion
         * @url https://switchbrew.org/wiki/System_Version_Title
         */
        struct SysVerTitle {
            u8 major; //!< The major version
            u8 minor; //!< The minor vision
            u8 micro; //!< The micro vision
            u8 _pad0_;
            u8 revMajor; //!< The major revision
            u8 revMinor; //!< The major revision
            u16 _pad1_;
            u8 platform[0x20]; //!< "NX"
            u8 verHash[0x40]; //!< The hash of the version string
            u8 dispVer[0x18]; //!< The version number string
            u8 dispTitle[0x80]; //!< The version title string
        };
        static_assert(sizeof(SysVerTitle) == 0x100);

      public:
        ISystemSettingsServer(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Writes the Firmware version to a 0xA buffer
         * @url https://switchbrew.org/wiki/Settings_services#GetFirmwareVersion
         */
        Result GetFirmwareVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Settings_services#GetColorSetId
         */
        Result GetColorSetId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x3, ISystemSettingsServer, GetFirmwareVersion),
            SFUNC(0x17, ISystemSettingsServer, GetColorSetId)
        )
    };
}
