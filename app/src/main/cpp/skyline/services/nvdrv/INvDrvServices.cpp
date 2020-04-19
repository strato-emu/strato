// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "devices/nvhost_ctrl.h"
#include "devices/nvhost_ctrl_gpu.h"
#include "devices/nvhost_channel.h"
#include "devices/nvhost_as_gpu.h"
#include "INvDrvServices.h"

namespace skyline::service::nvdrv {
    u32 INvDrvServices::OpenDevice(const std::string &path) {
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
            case (device::NvDeviceType::nvhost_ctrl):
                object = std::make_shared<device::NvHostCtrl>(state);
                break;
            case (device::NvDeviceType::nvhost_gpu):
            case (device::NvDeviceType::nvhost_vic):
            case (device::NvDeviceType::nvhost_nvdec):
                object = std::make_shared<device::NvHostChannel>(state, type);
                break;
            case (device::NvDeviceType::nvhost_ctrl_gpu):
                object = std::make_shared<device::NvHostCtrlGpu>(state);
                break;
            case (device::NvDeviceType::nvmap):
                object = std::make_shared<device::NvMap>(state);
                break;
            case (device::NvDeviceType::nvhost_as_gpu):
                object = std::make_shared<device::NvHostAsGpu>(state);
                break;
            default:
                throw exception("Cannot find NVDRV device");
        }
        deviceMap[type] = object;
        fdMap[fdIndex] = object;
        return fdIndex++;
    }

    INvDrvServices::INvDrvServices(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::nvdrv_INvDrvServices, "INvDrvServices", {
        {0x0, SFUNC(INvDrvServices::Open)},
        {0x1, SFUNC(INvDrvServices::Ioctl)},
        {0x2, SFUNC(INvDrvServices::Close)},
        {0x3, SFUNC(INvDrvServices::Initialize)},
        {0x4, SFUNC(INvDrvServices::QueryEvent)},
        {0x8, SFUNC(INvDrvServices::SetAruidByPID)}
    }) {}

    void INvDrvServices::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto buffer = request.inputBuf.at(0);
        auto path = state.process->GetString(buffer.address, buffer.size);
        response.Push<u32>(OpenDevice(path));
        response.Push<u32>(constant::status::Success);
    }

    void INvDrvServices::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto cmd = request.Pop<u32>();
        state.logger->Debug("IOCTL on device: 0x{:X}, cmd: 0x{:X}", fd, cmd);
        try {
            if (request.inputBuf.empty() || request.outputBuf.empty()) {
                if (request.inputBuf.empty()) {
                    device::IoctlData data(request.outputBuf.at(0));
                    fdMap.at(fd)->HandleIoctl(cmd, data);
                    response.Push<u32>(data.status);
                } else {
                    device::IoctlData data(request.inputBuf.at(0));
                    fdMap.at(fd)->HandleIoctl(cmd, data);
                    response.Push<u32>(data.status);
                }
            } else {
                device::IoctlData data(request.inputBuf.at(0), request.outputBuf.at(0));
                fdMap.at(fd)->HandleIoctl(cmd, data);
                response.Push<u32>(data.status);
            }
        } catch (const std::out_of_range &) {
            throw exception("IOCTL was requested on an invalid file descriptor");
        }
    }

    void INvDrvServices::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 fd = *reinterpret_cast<u32 *>(request.cmdArg);
        state.logger->Debug("Closing NVDRV device ({})", fd);
        try {
            auto device = fdMap.at(fd);
            if (!--device->refCount)
                deviceMap.erase(device->deviceType);
            fdMap.erase(fd);
        } catch (const std::out_of_range &) {
            state.logger->Warn("Trying to close non-existent FD");
        }
        response.Push<u32>(constant::status::Success);
    }

    void INvDrvServices::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(constant::status::Success);
    }

    void INvDrvServices::QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto eventId = request.Pop<u32>();
        auto event = std::make_shared<type::KEvent>(state);
        auto handle = state.process->InsertItem<type::KEvent>(event);
        state.logger->Debug("QueryEvent: FD: {}, Event ID: {}, Handle: {}", fd, eventId, handle);
        response.copyHandles.push_back(handle);
    }

    void INvDrvServices::SetAruidByPID(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(constant::status::Success);
    }
}
