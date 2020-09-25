// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "driver.h"
#include "devices/nvhost_ctrl.h"
#include "devices/nvhost_ctrl_gpu.h"
#include "devices/nvmap.h"
#include "devices/nvhost_channel.h"
#include "devices/nvhost_as_gpu.h"

namespace skyline::service::nvdrv {
    Driver::Driver(const DeviceState &state) : state(state), hostSyncpoint(state) {}

    u32 Driver::OpenDevice(std::string_view path) {
        state.logger->Debug("Opening NVDRV device ({}): {}", fdIndex, path);

        switch (util::Hash(path)) {
            #define NVDEVICE(type, name, devicePath)               \
                case util::Hash(devicePath): {                     \
                    std::shared_ptr<device::type> device{};        \
                    if (name.expired()) {                          \
                        device = device.make_shared(state);        \
                        name = device;                             \
                    } else {                                       \
                        device = name.lock();                      \
                    }                                              \
                    devices.push_back(device);                     \
                    break;                                         \
                }
            NVDEVICE_LIST
            #undef NVDEVICE

            default:
                throw exception("Cannot find NVDRV device");
        }

        return fdIndex++;
    }

    std::shared_ptr<device::NvDevice> Driver::GetDevice(u32 fd) {
        try {
            auto item = devices.at(fd);
            if (!item)
                throw exception("GetDevice was called with a closed file descriptor: 0x{:X}", fd);
            return item;
        } catch (std::out_of_range) {
            throw exception("GetDevice was called with invalid file descriptor: 0x{:X}", fd);
        }
    }

    void Driver::CloseDevice(u32 fd) {
        try {
            auto &device = devices.at(fd);
            device.reset();
        } catch (const std::out_of_range &) {
            state.logger->Warn("Trying to close non-existent FD");
        }
    }

    std::weak_ptr<Driver> driver{};
}
