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

    u32 Driver::OpenDevice(const std::string &path) {
        state.logger->Debug("Opening NVDRV device ({}): {}", fdIndex, path);
        auto type = device::nvDeviceMap.at(path);
        for (const auto &device : fdMap) {
            if (device.second->deviceType == type) {
                device.second->refCount++;
                fdMap[fdIndex] = device.second;
                return fdIndex++;
            }
        }

        std::shared_ptr<device::NvDevice> object;
        switch (type) {
            case device::NvDeviceType::nvhost_ctrl:
                object = std::make_shared<device::NvHostCtrl>(state);
                break;

            case device::NvDeviceType::nvhost_gpu:
            case device::NvDeviceType::nvhost_vic:
            case device::NvDeviceType::nvhost_nvdec:
                object = std::make_shared<device::NvHostChannel>(state, type);
                break;

            case device::NvDeviceType::nvhost_ctrl_gpu:
                object = std::make_shared<device::NvHostCtrlGpu>(state);
                break;

            case device::NvDeviceType::nvmap:
                object = std::make_shared<device::NvMap>(state);
                break;

            case device::NvDeviceType::nvhost_as_gpu:
                object = std::make_shared<device::NvHostAsGpu>(state);
                break;

            default:
                throw exception("Cannot find NVDRV device");
        }

        deviceMap[type] = object;
        fdMap[fdIndex] = object;

        return fdIndex++;
    }

    void Driver::CloseDevice(skyline::u32 fd) {
        try {
            auto& device = fdMap.at(fd);
            if (!--device->refCount)
                deviceMap.erase(device->deviceType);

            fdMap.erase(fd);
        } catch (const std::out_of_range &) {
            state.logger->Warn("Trying to close non-existent FD");
        }
    }

    std::weak_ptr<Driver> driver{};
}
