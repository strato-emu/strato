#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::kernel::service::set {
    /**
     * @brief set:sys or System Settings service provides access to system settings
     */
    class sys : public BaseService {
      private:
        /**
         * @brief Encapsulates the system version, this is sent to the application in GetFirmwareVersion (https://switchbrew.org/wiki/System_Version_Title)
         */
        struct SysVerTitle {
            u8 major; //!< The major version
            u8 minor; //!< The minor vision
            u8 micro; //!< The micro vision
            u8 : 8;
            u8 revMajor; //!< The major revision
            u8 revMinor; //!< The major revision
            u16 : 16;
            u8 platform[0x20]; //!< "NX"
            u8 verHash[0x40]; //!< This is the hash of the version string
            u8 dispVer[0x18]; //!< The version number string
            u8 dispTitle[0x80]; //!< The version title string
        };
        static_assert(sizeof(SysVerTitle) == 0x100);

      public:
        sys(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Writes the Firmware version to a 0xA buffer (https://switchbrew.org/wiki/Settings_services#GetFirmwareVersion)
         */
        void GetFirmwareVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
