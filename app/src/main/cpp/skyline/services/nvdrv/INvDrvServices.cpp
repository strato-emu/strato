// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "INvDrvServices.h"
#include "driver.h"
#include "devices/nvdevice.h"

namespace skyline::service::nvdrv {
    INvDrvServices::INvDrvServices(const DeviceState &state, ServiceManager &manager) : driver(nvdrv::driver.expired() ? std::make_shared<Driver>(state) : nvdrv::driver.lock()), BaseService(state, manager) {
        if (nvdrv::driver.expired())
            nvdrv::driver = driver;
    }

    Result INvDrvServices::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto path{request.inputBuf.at(0).as_string()};

        response.Push<u32>(driver->OpenDevice(path));
        response.Push(device::NvStatus::Success);

        return {};
    }

    Result INvDrvServices::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<u32>()};
        auto cmd{request.Pop<u32>()};

        auto device{driver->GetDevice(fd)};

        // Strip the permissions from the command leaving only the ID
        cmd &= 0xFFFF;

        span<u8> buffer{};
        if (request.inputBuf.empty() || request.outputBuf.empty()) {
            if (!request.inputBuf.empty())
                buffer = request.inputBuf.at(0);
            else if (!request.outputBuf.empty())
                buffer = request.outputBuf.at(0);
            else
                throw exception("No IOCTL Buffers");
        } else if (request.inputBuf[0].data() == request.outputBuf[0].data()) {
            if (request.inputBuf[0].size() >= request.outputBuf[0].size())
                buffer = request.inputBuf[0];
            else
                buffer = request.outputBuf[0];
        } else {
            throw exception("IOCTL Input Buffer (0x{:X}) != Output Buffer (0x{:X})", request.inputBuf[0].data(), request.outputBuf[0].data());
        }

        response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl, buffer, {}));
        return {};
    }

    Result INvDrvServices::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<u32>()};
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
        auto fd{request.Pop<u32>()};
        auto eventId{request.Pop<u32>()};

        auto device{driver->GetDevice(fd)};
        auto event{device->QueryEvent(eventId)};

        if (event != nullptr) {
            auto handle{state.process->InsertItem<type::KEvent>(event)};

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
        auto fd{request.Pop<u32>()};
        auto cmd{request.Pop<u32>()};

        auto device{driver->GetDevice(fd)};

        // Strip the permissions from the command leaving only the ID
        cmd &= 0xFFFF;

        if (request.inputBuf.size() < 2 || request.outputBuf.empty())
            throw exception("Inadequate amount of buffers for IOCTL2: I - {}, O - {}", request.inputBuf.size(), request.outputBuf.size());
        else if (request.inputBuf[0].data() != request.outputBuf[0].data())
            throw exception("IOCTL2 Input Buffer (0x{:X}) != Output Buffer (0x{:X}) [Input Buffer #2: 0x{:X}]", request.inputBuf[0].data(), request.outputBuf[0].data(), request.inputBuf[1].data());

        span<u8> buffer{};
        if (request.inputBuf[0].size() >= request.outputBuf[0].size())
            buffer = request.inputBuf[0];
        else
            buffer = request.outputBuf[0];

        response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl2, buffer, request.inputBuf[1]));
        return {};
    }

    Result INvDrvServices::Ioctl3(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<u32>()};
        auto cmd{request.Pop<u32>()};

        auto device{driver->GetDevice(fd)};

        // Strip the permissions from the command leaving only the ID
        cmd &= 0xFFFF;

        if (request.inputBuf.empty() || request.outputBuf.size() < 2)
            throw exception("Inadequate amount of buffers for IOCTL3: I - {}, O - {}", request.inputBuf.size(), request.outputBuf.size());
        else if (request.inputBuf[0].data() != request.outputBuf[0].data())
            throw exception("IOCTL3 Input Buffer (0x{:X}) != Output Buffer (0x{:X}) [Output Buffer #2: 0x{:X}]", request.inputBuf[0].data(), request.outputBuf[0].data(), request.outputBuf[1].data());

        span<u8> buffer{};
        if (request.inputBuf[0].size() >= request.outputBuf[0].size())
            buffer = request.inputBuf[0];
        else
            buffer = request.outputBuf[0];

        response.Push(device->HandleIoctl(cmd, device::IoctlType::Ioctl3, buffer, request.outputBuf[1]));
        return {};
    }

    Result INvDrvServices::SetGraphicsFirmwareMemoryMarginEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
