// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "driver.h"
#include "devices/nvmap.h"
#include "devices/nvhost/ctrl.h"
#include "devices/nvhost/ctrl_gpu.h"
#include "devices/nvhost/gpu_channel.h"
#include "devices/nvhost/as_gpu.h"
#include "devices/nvhost/host1x_channel.h"

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

        #define DEVICE_CASE(path, object, ...)                                                                     \
            case util::Hash(path):                                                                                 \
                {                                                                                                  \
                    std::unique_lock lock(deviceMutex);                                                            \
                    devices.emplace(fd, std::make_unique<device::object>(state, *this, core, ctx, ##__VA_ARGS__)); \
                    return NvResult::Success;                                                                      \
                }

        DEVICE_SWITCH(
            DEVICE_CASE("/dev/nvmap", NvMap)
            DEVICE_CASE("/dev/nvhost-ctrl", nvhost::Ctrl)
        )

        if (ctx.perms.AccessGpu) {
            DEVICE_SWITCH(
                DEVICE_CASE("/dev/nvhost-as-gpu", nvhost::AsGpu)
                DEVICE_CASE("/dev/nvhost-ctrl-gpu", nvhost::CtrlGpu)
                DEVICE_CASE("/dev/nvhost-gpu", nvhost::GpuChannel)
            )
        }

        if (ctx.perms.AccessJpeg)
            DEVICE_SWITCH(DEVICE_CASE("/dev/nvhost-nvjpg", nvhost::Host1xChannel, core::ChannelType::NvJpg))

        if (ctx.perms.AccessVic)
            DEVICE_SWITCH(DEVICE_CASE("/dev/nvhost-vic", nvhost::Host1xChannel, core::ChannelType::Vic))

        if (ctx.perms.AccessVideoDecoder)
            DEVICE_SWITCH(DEVICE_CASE("/dev/nvhost-nvdec", nvhost::Host1xChannel, core::ChannelType::NvDec))

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

    NvResult Driver::Ioctl(FileDescriptor fd, IoctlDescriptor cmd, span<u8> buffer) {
        state.logger->Debug("fd: {}, cmd: 0x{:X}, device: {}", fd, cmd.raw, devices.at(fd)->GetName());

        try {
            std::shared_lock lock(deviceMutex);
            return ConvertResult(devices.at(fd)->Ioctl(cmd, buffer));
        } catch (const std::out_of_range &) {
            throw exception("Ioctl was called with invalid fd: {}", fd);
        }
    }

    NvResult Driver::Ioctl2(FileDescriptor fd, IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer) {
        state.logger->Debug("fd: {}, cmd: 0x{:X}, device: {}", fd, cmd.raw, devices.at(fd)->GetName());

        try {
            std::shared_lock lock(deviceMutex);
            return ConvertResult(devices.at(fd)->Ioctl2(cmd, buffer, inlineBuffer));
        } catch (const std::out_of_range &) {
            throw exception("Ioctl2 was called with invalid fd: {}", fd);
        }
    }

    NvResult Driver::Ioctl3(FileDescriptor fd, IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer) {
        state.logger->Debug("fd: {}, cmd: 0x{:X}, device: {}", fd, cmd.raw, devices.at(fd)->GetName());

        try {
            std::shared_lock lock(deviceMutex);
            return ConvertResult(devices.at(fd)->Ioctl3(cmd, buffer, inlineBuffer));
        } catch (const std::out_of_range &) {
            throw exception("Ioctl3 was called with invalid fd: {}", fd);
        }
    }

    void Driver::CloseDevice(FileDescriptor fd) {
        try {
            std::unique_lock lock(deviceMutex);
            devices.erase(fd);
        } catch (const std::out_of_range &) {
            state.logger->Warn("Trying to close invalid fd: {}");
        }
    }

    std::shared_ptr<kernel::type::KEvent> Driver::QueryEvent(FileDescriptor fd, u32 eventId) {
        state.logger->Debug("fd: {}, eventId: 0x{:X}, device: {}", fd, eventId, devices.at(fd)->GetName());

        try {
            std::shared_lock lock(deviceMutex);
            return devices.at(fd)->QueryEvent(eventId);
        } catch (const std::exception &) {
            throw exception("QueryEvent was called with invalid fd: {}", fd);
        }
    }
}
