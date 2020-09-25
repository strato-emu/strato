// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "devices/nvhost_syncpoint.h"

#define NVDEVICE_LIST                                              \
    NVDEVICE(NvHostCtrl,    nvHostCtrl,    "/dev/nvhost-ctrl")     \
    NVDEVICE(NvHostChannel, nvHostGpu,     "/dev/nvhost-gpu")      \
    NVDEVICE(NvHostChannel, nvHostNvdec,   "/dev/nvhost-nvdec")    \
    NVDEVICE(NvHostChannel, nvHostVic,     "/dev/nvhost-vic")      \
    NVDEVICE(NvMap,         nvMap,         "/dev/nvmap")           \
    NVDEVICE(NvHostAsGpu,   nvHostAsGpu,   "/dev/nvhost-as-gpu")   \
    NVDEVICE(NvHostCtrlGpu, nvHostCtrlGpu, "/dev/nvhost-ctrl-gpu")

namespace skyline::service::nvdrv {
    namespace device {
        class NvDevice;

        #define NVDEVICE(type, name, path) class type;
        NVDEVICE_LIST
        #undef NVDEVICE
    }

    /**
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is responsible for writing buffers to the display
     */
    class Driver {
      private:
        const DeviceState &state;
        std::vector<std::shared_ptr<device::NvDevice>> devices; //!< A vector of shared pointers to NvDevice object that correspond to FDs
        u32 fdIndex{}; //!< The next file descriptor to assign

      public:
        NvHostSyncpoint hostSyncpoint;

        #define NVDEVICE(type, name, path) std::weak_ptr<device::type> name;
        NVDEVICE_LIST
        #undef NVDEVICE

        Driver(const DeviceState &state);

        /**
         * @brief Open a specific device and return a FD
         * @param path The path of the device to open an FD to
         * @return The file descriptor to the device
         */
        u32 OpenDevice(std::string_view path);

        /**
         * @brief Returns a particular device with a specific FD
         * @param fd The file descriptor to retrieve
         * @return A shared pointer to the device
         */
        std::shared_ptr<device::NvDevice> GetDevice(u32 fd);

        /**
         * @brief Returns a particular device with a specific FD
         * @tparam objectClass The class of the device to return
         * @param fd The file descriptor to retrieve
         * @return A shared pointer to the device
         */
        template<typename objectClass>
        inline std::shared_ptr<objectClass> GetDevice(u32 fd) {
            return std::static_pointer_cast<objectClass>(GetDevice(fd));
        }

        /**
         * @brief Closes the specified device with it's file descriptor
         */
        void CloseDevice(u32 fd);
    };

    extern std::weak_ptr<Driver> driver; //!< A globally shared instance of the Driver
}
