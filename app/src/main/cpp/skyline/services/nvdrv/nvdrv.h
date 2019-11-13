#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <kernel/types/KTransferMemory.h>
#include <gpu.h>

namespace skyline::kernel::service::nvdrv {
    /**
     * @brief nvdrv or INvDrvServices is used to access the Nvidia GPU inside the Switch (https://switchbrew.org/wiki/NV_services#nvdrv.2C_nvdrv:a.2C_nvdrv:s.2C_nvdrv:t)
     */
    class nvdrv : public BaseService {
      public:
        nvdrv(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Open a specific device and return a FD (https://switchbrew.org/wiki/NV_services#Open)
         */
        void Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Close the specified FD (https://switchbrew.org/wiki/NV_services#Close)
         */
        void Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Close the specified FD (https://switchbrew.org/wiki/NV_services#Close)
         */
        void Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This initializes the driver (https://switchbrew.org/wiki/NV_services#Initialize)
         */
        void Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a specific event from a device (https://switchbrew.org/wiki/NV_services#QueryEvent)
         */
        void QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the AppletResourceUserId which matches the PID (https://switchbrew.org/wiki/NV_services#SetAruidByPID)
         */
        void SetAruidByPID(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
