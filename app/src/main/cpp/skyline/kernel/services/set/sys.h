#pragma once

#include "../base_service.h"
#include "../serviceman.h"

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
            u8 major;
            u8 minor;
            u8 micro;
            u8 : 8;
            u8 rev_major;
            u8 rev_minor;
            u16 : 16;
            u8 platform[0x20];
            u8 ver_hash[0x40];
            u8 disp_ver[0x18];
            u8 disp_title[0x80];
        };
        static_assert(sizeof(SysVerTitle) == 0x100);

      public:
        sys(const DeviceState &state, ServiceManager& manager);

        /**
         * @brief Writes the Firmware version to a 0xA buffer (https://switchbrew.org/wiki/Settings_services#GetFirmwareVersion)
         */
        void GetFirmwareVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
