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
        {0x8, SFUNC(INvDrvServices::SetAruid)},
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
            throw exception("IOCTL Input Buffer (0x{:X}) != Output Buffer (0x{:X})", request.inputBuf[0].address, request.outputBuf[0].address);
        }

        response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl, std::span<u8>(reinterpret_cast<u8 *>(buffer->address), buffer->size), {}));
        return {};
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

    Result INvDrvServices::SetAruid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(device::NvStatus::Success);
        return {};
    }

    Result INvDrvServices::Ioctl2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto cmd = request.Pop<u32>();

        auto device = driver->GetDevice(fd);

        // Strip the permissions from the command leaving only the ID
        cmd &= 0xFFFF;

        if (request.inputBuf.size() < 2 || request.outputBuf.empty())
            throw exception("Inadequate amount of buffers for IOCTL2: I - {}, O - {}", request.inputBuf.size(), request.outputBuf.size());
        else if (request.inputBuf[0].address != request.outputBuf[0].address)
            throw exception("IOCTL2 Input Buffer (0x{:X}) != Output Buffer (0x{:X}) [Input Buffer #2: 0x{:X}]", request.inputBuf[0].address, request.outputBuf[0].address, request.inputBuf[1].address);

        response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl2, std::span<u8>(reinterpret_cast<u8 *>(request.inputBuf[0].address), request.inputBuf[0].size), std::span<u8>(reinterpret_cast<u8 *>(request.inputBuf[1].address), request.inputBuf[1].size)));
        return {};
    }

    Result INvDrvServices::Ioctl3(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto cmd = request.Pop<u32>();

        auto device = driver->GetDevice(fd);

        // Strip the permissions from the command leaving only the ID
        cmd &= 0xFFFF;

        if (request.inputBuf.empty() || request.outputBuf.size() < 2)
            throw exception("Inadequate amount of buffers for IOCTL3: I - {}, O - {}", request.inputBuf.size(), request.outputBuf.size());
        else if (request.inputBuf[0].address != request.outputBuf[0].address)
            throw exception("IOCTL3 Input Buffer (0x{:X}) != Output Buffer (0x{:X}) [Output Buffer #2: 0x{:X}]", request.inputBuf[0].address, request.outputBuf[0].address, request.outputBuf[1].address);

        response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl3, std::span<u8>(reinterpret_cast<u8 *>(request.inputBuf[0].address), request.inputBuf[0].size), std::span<u8>(reinterpret_cast<u8 *>(request.outputBuf[1].address), request.outputBuf[1].size)));
        return {};
    }

    Result INvDrvServices::SetGraphicsFirmwareMemoryMarginEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
