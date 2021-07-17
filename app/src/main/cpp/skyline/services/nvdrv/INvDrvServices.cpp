// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "INvDrvServices.h"
#include "driver.h"
#include "devices/nvdevice.h"

#define NVRESULT(x) [&response](NvResult err) { response.Push<NvResult>(err); return Result{}; }(x)

namespace skyline::service::nvdrv {
    INvDrvServices::INvDrvServices(const DeviceState &state, ServiceManager &manager, Driver &driver, const SessionPermissions &perms) : BaseService(state, manager), driver(driver), ctx(SessionContext{.perms = perms}) {}

    Result INvDrvServices::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr FileDescriptor SessionFdLimit{sizeof(u64) * 2 * 8}; //!< Nvdrv uses two 64 bit variables to store a bitset

        auto path{request.inputBuf.at(0).as_string(true)};
        if (path.empty() || nextFdIndex == SessionFdLimit) {
            response.Push<FileDescriptor>(InvalidFileDescriptor);
            return NVRESULT(NvResult::FileOperationFailed);
        }

        if (auto err{driver.OpenDevice(path, nextFdIndex, ctx)}; err != NvResult::Success) {
            response.Push<FileDescriptor>(InvalidFileDescriptor);
            return NVRESULT(err);
        }

        response.Push(nextFdIndex++);
        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<FileDescriptor>()};
        auto ioctl{request.Pop<IoctlDescriptor>()};

        auto inBuf{request.inputBuf.at(0)};
        auto outBuf{request.outputBuf.at(0)};

        if (ioctl.in && inBuf.size() < ioctl.size)
            return NVRESULT(NvResult::InvalidSize);

        if (ioctl.out && outBuf.size() < ioctl.size)
            return NVRESULT(NvResult::InvalidSize);

        if (ioctl.in && ioctl.out) {
            if (outBuf.size() < inBuf.size())
                return NVRESULT(NvResult::InvalidSize);

            // Copy in buf to out buf for inout ioctls to avoid needing to pass around two buffers everywhere
            if (outBuf.data() != inBuf.data())
                outBuf.copy_from(inBuf, ioctl.size);
        }

        return NVRESULT(driver.Ioctl(fd, ioctl, ioctl.out ? outBuf : inBuf));
    }

    Result INvDrvServices::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<u32>()};
        state.logger->Debug("Closing NVDRV device ({})", fd);

        driver.CloseDevice(fd);

        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<u32>()};
        auto eventId{request.Pop<u32>()};

        auto event{driver.QueryEvent(fd, eventId)};

        if (event != nullptr) {
            auto handle{state.process->InsertItem<type::KEvent>(event)};

            state.logger->Debug("FD: {}, Event ID: {}, Handle: 0x{:X}", fd, eventId, handle);
            response.copyHandles.push_back(handle);

            return NVRESULT(NvResult::Success);
        } else {
            return NVRESULT(NvResult::BadValue);
        }
    }

    Result INvDrvServices::SetAruid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return NVRESULT(NvResult::Success);
    }


    Result INvDrvServices::SetGraphicsFirmwareMemoryMarginEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
