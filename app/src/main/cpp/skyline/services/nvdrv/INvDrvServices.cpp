// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <numeric>
#include <kernel/types/KProcess.h>
#include "INvDrvServices.h"
#include "driver.h"
#include "devices/nvdevice.h"

#define NVRESULT(x) [&response](NvResult err) {         \
        if (err != NvResult::Success)                   \
            Logger::Debug("IOCTL Failed: 0x{:X}", err); \
        response.Push<NvResult>(err);                   \
        return Result{};                                \
    } (x)

namespace skyline::service::nvdrv {
    INvDrvServices::INvDrvServices(const DeviceState &state, ServiceManager &manager, Driver &driver, const SessionPermissions &perms)
        : BaseService(state, manager),
          driver(driver),
          ctx(SessionContext{.perms = perms}) {}

    Result INvDrvServices::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr FileDescriptor SessionFdLimit{std::numeric_limits<u64>::digits * 2}; //!< Nvdrv uses two 64 bit variables to store a bitset

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

    static NvResultValue<span<u8>> GetMainIoctlBuffer(IoctlDescriptor ioctl, span<u8> inBuf, span<u8> outBuf) {
        if (ioctl.in && inBuf.size() < ioctl.size)
            return NvResult::InvalidSize;

        if (ioctl.out && outBuf.size() < ioctl.size)
            return NvResult::InvalidSize;

        if (ioctl.in && ioctl.out) {
            if (outBuf.size() < inBuf.size())
                return NvResult::InvalidSize;

            // Copy in buf to out buf for inout ioctls to avoid needing to pass around two buffers everywhere
            if (outBuf.data() != inBuf.data())
                outBuf.copy_from(inBuf, ioctl.size);
        }

        return ioctl.out ? outBuf : inBuf;
    }

    Result INvDrvServices::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<FileDescriptor>()};
        auto ioctl{request.Pop<IoctlDescriptor>()};

        auto buf{GetMainIoctlBuffer(ioctl,
                                    !request.inputBuf.empty() ? request.inputBuf.at(0) : span<u8>{},
                                    !request.outputBuf.empty() ? request.outputBuf.at(0) : span<u8>{})};
        if (!buf)
            return NVRESULT(buf);
        else
            return NVRESULT(driver.Ioctl(fd, ioctl, *buf));
    }

    Result INvDrvServices::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<FileDescriptor>()};
        Logger::Debug("Closing NVDRV device ({})", fd);

        driver.CloseDevice(fd);

        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<FileDescriptor>()};
        auto eventId{request.Pop<u32>()};

        auto event{driver.QueryEvent(fd, eventId)};

        if (event != nullptr) {
            auto handle{state.process->InsertItem<type::KEvent>(event)};

            Logger::Debug("FD: {}, Event ID: {}, Handle: 0x{:X}", fd, eventId, handle);
            response.copyHandles.push_back(handle);

            return NVRESULT(NvResult::Success);
        } else {
            return NVRESULT(NvResult::BadValue);
        }
    }

    Result INvDrvServices::Ioctl2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<FileDescriptor>()};
        auto ioctl{request.Pop<IoctlDescriptor>()};

        // Inline buffer is optional
        auto inlineBuf{request.inputBuf.size() > 1 ? request.inputBuf.at(1) : span<u8>{}};

        auto buf{GetMainIoctlBuffer(ioctl,
                                    !request.inputBuf.empty() ? request.inputBuf.at(0) : span<u8>{},
                                    !request.outputBuf.empty() ? request.outputBuf.at(0) : span<u8>{})};
        if (!buf)
            return NVRESULT(buf);
        else
            return NVRESULT(driver.Ioctl2(fd, ioctl, *buf, inlineBuf));
    }

    Result INvDrvServices::Ioctl3(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd{request.Pop<FileDescriptor>()};
        auto ioctl{request.Pop<IoctlDescriptor>()};

        // Inline buffer is optional
        auto inlineBuf{request.outputBuf.size() > 1 ? request.outputBuf.at(1) : span<u8>{}};

        auto buf{GetMainIoctlBuffer(ioctl,
                                    !request.inputBuf.empty() ? request.inputBuf.at(0) : span<u8>{},
                                    !request.outputBuf.empty() ? request.outputBuf.at(0) : span<u8>{})};
        if (!buf)
            return NVRESULT(buf);
        else
            return NVRESULT(driver.Ioctl3(fd, ioctl, *buf, inlineBuf));
    }

    Result INvDrvServices::GetStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct Status {
            u32 freeSize;
            u32 allocatableSize;
            u32 minimumFreeSize;
            u32 minimumAllocatableSize;
            u32 reserved;
        };

        // Return empty values since we don't use the transfer memory for allocations
        response.Push<Status>({});
        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::SetAruid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return NVRESULT(NvResult::Success);
    }

    Result INvDrvServices::DumpStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result INvDrvServices::SetGraphicsFirmwareMemoryMarginEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
