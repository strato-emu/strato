// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "devices/nvhost_syncpoint.h"

namespace skyline::service::nvdrv {
    namespace device {
        class NvDevice;
        enum class NvDeviceType;
    }

    /**
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is responsible for writing buffers to the display
     */
    class Driver {
      private:
        const DeviceState &state;
        std::unordered_map<device::NvDeviceType, std::shared_ptr<device::NvDevice>> deviceMap; //!< A map from a NvDeviceType to the NvDevice object
        std::unordered_map<u32, std::shared_ptr<device::NvDevice>> fdMap; //!< A map from an FD to a shared pointer to it's NvDevice object
        u32 fdIndex{}; //!< The index of a file descriptor

      public:
        NvHostSyncpoint hostSyncpoint;

        Driver(const DeviceState &state);

        /**
         * @brief Open a specific device and return a FD
         * @param path The path of the device to open an FD to
         * @return The file descriptor to the device
         */
        u32 OpenDevice(const std::string &path);

        /**
         * @brief Closes the specified device with it's file descriptor
         */
        void CloseDevice(u32 fd);

        /**
         * @brief Returns a particular device with a specific FD
         * @tparam objectClass The class of the device to return
         * @param fd The file descriptor to retrieve
         * @return A shared pointer to the device
         */
        template<typename objectClass = device::NvDevice>
        std::shared_ptr<objectClass> GetDevice(u32 fd) {
            try {
                auto item = fdMap.at(fd);
                return std::static_pointer_cast<objectClass>(item);
            } catch (std::out_of_range) {
                throw exception("GetDevice was called with invalid file descriptor: 0x{:X}", fd);
            }
        }

        /**
         * @brief Returns a particular device with a specific type
         * @tparam objectClass The class of the device to return
         * @param type The type of the device to return
         * @return A shared pointer to the device
         */
        template<typename objectClass = device::NvDevice>
        std::shared_ptr<objectClass> GetDevice(device::NvDeviceType type) {
            try {
                auto item = deviceMap.at(type);
                return std::static_pointer_cast<objectClass>(item);
            } catch (std::out_of_range) {
                throw exception("GetDevice was called with invalid type: 0x{:X}", type);
            }
        }
    };

    extern std::weak_ptr<Driver> driver; //!< A globally shared instance of the Driver
}
