// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "INvDrvServices.h"
#include "driver.h"
#include "devices/nvdevice.h"

namespace skyline::service::nvdrv {
    INvDrvServices::INvDrvServices(const DeviceState &state, ServiceManager &manager) : driver(nvdrv::driver.expired() ? std::make_shared<Driver>(state) : nvdrv::driver.lock()), BaseService(state, manager, {
        {0x0, SFUNC(INvDrvServices::Open)},
        {0x1, SFUNC(INvDrvServices::Ioctl)},
        {0x2, SFUNC(INvDrvServices::Close)},
        {0x3, SFUNC(INvDrvServices::Initialize)},
        {0x4, SFUNC(INvDrvServices::QueryEvent)},
        {0x8, SFUNC(INvDrvServices::SetAruidByPID)},
        {0xD, SFUNC(INvDrvServices::SetGraphicsFirmwareMemoryMarginEnabled)}
    }) {
        if (nvdrv::driver.expired())
            nvdrv::driver = driver;
    }

    Result INvDrvServices::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto buffer = request.inputBuf.at(0);
        auto path = state.process->GetString(buffer.address, buffer.size);

        response.Push<u32>(driver->OpenDevice(path));
        response.Push(device::NvStatus::Success);

        return {};
    }

    Result INvDrvServices::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto cmd = request.Pop<u32>();

        auto device = driver->GetDevice(fd);

        // Strip the permissions from the command leaving only the ID
        cmd &= 0xFFFF;

        try {
            std::optional<kernel::ipc::IpcBuffer> buffer{std::nullopt};
            if (request.inputBuf.empty() || request.outputBuf.empty()) {
                if (!request.inputBuf.empty())
                    buffer = request.inputBuf.at(0);
                else if (!request.outputBuf.empty())
                    buffer = request.outputBuf.at(0);
                else
                    throw exception("No IOCTL Buffers");
            } else if (request.inputBuf.at(0).address == request.outputBuf.at(0).address) {
                buffer = request.inputBuf.at(0);
            } else {
                throw exception("IOCTL Input Buffer != Output Buffer");
            }

            response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl, std::span<u8>(reinterpret_cast<u8 *>(buffer->address), buffer->size), {}));
            return {};
        } catch (const std::out_of_range &) {
            throw exception("IOCTL was requested on an invalid file descriptor: 0x{:X}", fd);
        }
    }

    Result INvDrvServices::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        state.logger->Debug("Closing NVDRV device ({})", fd);

        driver->CloseDevice(fd);

        response.Push(device::NvStatus::Success);
        return {};
    }

    Result INvDrvServices::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(device::NvStatus::Success);
        return {};
    }

    Result INvDrvServices::QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto eventId = request.Pop<u32>();

        auto device = driver->GetDevice(fd);
        auto event = device->QueryEvent(eventId);

        if (event != nullptr) {
            auto handle = state.process->InsertItem<type::KEvent>(event);

            state.logger->Debug("QueryEvent: FD: {}, Event ID: {}, Handle: {}", fd, eventId, handle);
            response.copyHandles.push_back(handle);

            response.Push(device::NvStatus::Success);
        } else {
            response.Push(device::NvStatus::BadValue);
        }

        return {};
    }

    Result INvDrvServices::SetAruidByPID(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(device::NvStatus::Success);
        return {};
    }

    Result INvDrvServices::SetGraphicsFirmwareMemoryMarginEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
