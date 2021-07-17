// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "driver.h"
#include "devices/nvmap.h"
#include "devices/nvhost/ctrl.h"
#include "devices/nvhost/ctrl_gpu.h"
#include "devices/nvhost/gpu_channel.h"
#include "devices/nvhost/as_gpu.h"


namespace skyline::service::nvdrv {
    Driver::Driver(const DeviceState &state) : state(state), core(state) {}

    NvResult Driver::OpenDevice(std::string_view path, FileDescriptor fd, const SessionContext &ctx) {
        state.logger->Debug("Opening NvDrv device ({}): {}", fd, path);
        auto pathHash{util::Hash(path)};

        #define DEVICE_SWITCH(cases) \
            switch (pathHash) {      \
                cases;               \
                default:             \
                    break;           \
            }

        #define DEVICE_CASE(path, object) \
            case util::Hash(path): \
                devices.emplace(fd, std::make_unique<device::object>(state, core, ctx)); \
                return NvResult::Success;

        DEVICE_SWITCH(
            DEVICE_CASE("/dev/nvmap", NvMap)
            DEVICE_CASE("/dev/nvhost-ctrl", nvhost::Ctrl)
        );

        if (ctx.perms.AccessGpu) {
            DEVICE_SWITCH(
                DEVICE_CASE("/dev/nvhost-as-gpu", nvhost::AsGpu)
                DEVICE_CASE("/dev/nvhost-ctrl-gpu", nvhost::CtrlGpu)
                DEVICE_CASE("/dev/nvhost-gpu", nvhost::GpuChannel)
            );
        }/*

        if (ctx.perms.AccessVic) {
            switch (pathHash) {
                ENTRY("/dev/nvhost-vic", nvhost::Vic)
                default:
                    break;
            }
        }

        if (ctx.perms.AccessVideoDecoder) {
            switch (pathHash) {
                ENTRY("/dev/nvhost-nvdec", nvhost::NvDec)
                default:
                    break;
            }
        }

        if (ctx.perms.AccessJpeg) {
            switch (pathHash) {
                ENTRY("/dev/nvhost-nvjpg", nvhost::NvJpg)
                default:
                    break;
            }
        }*/

        #undef DEVICE_CASE
        #undef DEVICE_SWITCH

        // Device doesn't exist/no permissions
        return NvResult::FileOperationFailed;
    }

    static NvResult ConvertResult(PosixResult result) {
        switch (result) {
            case PosixResult::Success:
                return NvResult::Success;
            case PosixResult::NotPermitted:
                return NvResult::AccessDenied;
            case PosixResult::TryAgain:
                return NvResult::Timeout;
            case PosixResult::Busy:
                return NvResult::Busy;
            case PosixResult::InvalidArgument:
                return NvResult::BadValue;
            case PosixResult::InappropriateIoctlForDevice:
                return NvResult::IoctlFailed;
            case PosixResult::NotSupported:
                return NvResult::NotSupported;
            case PosixResult::TimedOut:
                return NvResult::Timeout;
            default:
                throw exception("Unhandled POSIX result: {}!", static_cast<i32>(result));
        }

    }

    NvResult Driver::Ioctl(u32 fd, IoctlDescriptor cmd, span<u8> buffer) {
        state.logger->Debug("fd: {}, cmd: 0x{:X}, device: {}", fd, cmd.raw, devices.at(fd)->GetName());

        try {
            return ConvertResult(devices.at(fd)->Ioctl(cmd, buffer));
        } catch (const std::out_of_range &) {
            throw exception("Ioctl was called with invalid file descriptor: 0x{:X}", fd);
        }
    }

    NvResult Driver::Ioctl2(u32 fd, IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer) {
        state.logger->Debug("fd: {}, cmd: 0x{:X}, device: {}", fd, cmd.raw, devices.at(fd)->GetName());

        try {
            return ConvertResult(devices.at(fd)->Ioctl2(cmd, buffer, inlineBuffer));
        } catch (const std::out_of_range &) {
            throw exception("Ioctl2 was called with invalid file descriptor: 0x{:X}", fd);
        }
    }

    NvResult Driver::Ioctl3(u32 fd, IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer) {
        state.logger->Debug("fd: {}, cmd: 0x{:X}, device: {}", fd, cmd.raw, devices.at(fd)->GetName());

        try {
            return ConvertResult(devices.at(fd)->Ioctl3(cmd, buffer, inlineBuffer));
        } catch (const std::out_of_range &) {
            throw exception("Ioctl3 was called with invalid file descriptor: 0x{:X}", fd);
        }
    }

    void Driver::CloseDevice(u32 fd) {
        try {
            devices.at(fd).reset();
        } catch (const std::out_of_range &) {
            state.logger->Warn("Trying to close non-existent FD");
        }
    }

    std::shared_ptr<kernel::type::KEvent> Driver::QueryEvent(u32 fd, u32 eventId) {
        state.logger->Debug("fd: {}, eventId: 0x{:X}, device: {}", fd, eventId, devices.at(fd)->GetName());

        try {
            return devices.at(fd)->QueryEvent(eventId);
        } catch (const std::exception &) {
            throw exception("QueryEvent was called with invalid file descriptor: 0x{:X}", fd);
        }
    }
}
