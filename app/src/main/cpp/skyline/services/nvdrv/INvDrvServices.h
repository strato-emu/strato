// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <kernel/types/KTransferMemory.h>
#include <gpu.h>
#include "devices/nvdevice.h"

namespace skyline::service::nvdrv {
    /**
     * @brief nvdrv or INvDrvServices is used to access the Nvidia GPU inside the Switch (https://switchbrew.org/wiki/NV_services#nvdrv.2C_nvdrv:a.2C_nvdrv:s.2C_nvdrv:t)
     */
    class INvDrvServices : public BaseService {
      private:
        std::unordered_map<device::NvDeviceType, std::shared_ptr<device::NvDevice>> deviceMap; //!< A map from a NvDeviceType to the NvDevice object
        std::unordered_map<u32, std::shared_ptr<device::NvDevice>> fdMap; //!< A map from an FD to a shared pointer to it's NvDevice object
        u32 fdIndex{}; //!< Holds the index of a file descriptor

        /**
         * @brief Open a specific device and return a FD
         * @param path The path of the device to open an FD to
         * @return The file descriptor to the device
         */
        u32 OpenDevice(const std::string &path);

        /**
         * @brief Returns a particular device with a specific FD
         * @tparam objectClass The class of the device to return
         * @param fd The file descriptor to retrieve
         * @return A shared pointer to the device
         */
        template<typename objectClass>
        std::shared_ptr<objectClass> GetDevice(u32 fd) {
            try {
                auto item = fdMap.at(fd);
                return std::static_pointer_cast<objectClass>(item);
            } catch (std::out_of_range) {
                throw exception("GetDevice was called with invalid file descriptor: 0x{:X}", fd);
            }
        }

      public:
        /**
         * @brief Returns a particular device with a specific type
         * @tparam objectClass The class of the device to return
         * @param type The type of the device to return
         * @return A shared pointer to the device
         */
        template<typename objectClass>
        std::shared_ptr<objectClass> GetDevice(device::NvDeviceType type) {
            try {
                auto item = deviceMap.at(type);
                return std::static_pointer_cast<objectClass>(item);
            } catch (std::out_of_range) {
                throw exception("GetDevice was called with invalid type: 0x{:X}", type);
            }
        }

        INvDrvServices(const DeviceState &state, ServiceManager &manager);

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
