// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include "types.h"

namespace skyline::service::nvdrv {
    class Driver;

    /**
     * @brief nvdrv or INvDrvServices is used to access the Nvidia GPU inside the Switch
     * @url https://switchbrew.org/wiki/NV_services#nvdrv.2C_nvdrv:a.2C_nvdrv:s.2C_nvdrv:t
     */
    class INvDrvServices : public BaseService {
      private:
        Driver &driver; //!< The global nvdrv driver this session accesses
        SessionContext ctx; //!< Session specific context

        FileDescriptor nextFdIndex{1}; //!< The index for the next allocated file descriptor

      public:
        INvDrvServices(const DeviceState &state, ServiceManager &manager, Driver &driver, const SessionPermissions &perms);

        /**
         * @brief Open a specific device and return a FD
         * @url https://switchbrew.org/wiki/NV_services#Open
         */
        Result Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Perform an IOCTL on the specified FD
         * @url https://switchbrew.org/wiki/NV_services#Ioctl
         */
        Result Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Close the specified FD
         * @url https://switchbrew.org/wiki/NV_services#Close
         */
        Result Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Initializes the driver
         * @url https://switchbrew.org/wiki/NV_services#Initialize
         */
        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a specific event from a device
         * @url https://switchbrew.org/wiki/NV_services#QueryEvent
         */
        Result QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns info about the usage of the transfer memory by the internal allocator
         * @url https://switchbrew.org/wiki/NV_services#GetStatus
         */
        Result GetStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the AppletResourceUserId which matches the PID
         * @url https://switchbrew.org/wiki/NV_services#SetAruid
         */
        Result SetAruid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/NV_services#DumpStatus
         */
        Result DumpStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Perform an IOCTL on the specified FD with an extra input buffer
         * @url https://switchbrew.org/wiki/NV_services#Ioctl2
         */
        Result Ioctl2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Perform an IOCTL on the specified FD with an extra output buffer
         * @url https://switchbrew.org/wiki/NV_services#Ioctl3
         */
        Result Ioctl3(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Enables the graphics firmware memory margin
         * @url https://switchbrew.org/wiki/NV_services#SetGraphicsFirmwareMemoryMarginEnabled
         */
        Result SetGraphicsFirmwareMemoryMarginEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, INvDrvServices, Open),
            SFUNC(0x1, INvDrvServices, Ioctl),
            SFUNC(0x2, INvDrvServices, Close),
            SFUNC(0x3, INvDrvServices, Initialize),
            SFUNC(0x4, INvDrvServices, QueryEvent),
            SFUNC(0x6, INvDrvServices, GetStatus),
            SFUNC(0x8, INvDrvServices, SetAruid),
            SFUNC(0x9, INvDrvServices, DumpStatus),
            SFUNC(0xB, INvDrvServices, Ioctl2),
            SFUNC(0xC, INvDrvServices, Ioctl3),
            SFUNC(0xD, INvDrvServices, SetGraphicsFirmwareMemoryMarginEnabled)
        )
    };
}
