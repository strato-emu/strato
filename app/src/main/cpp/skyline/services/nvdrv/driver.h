// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <kernel/types/KEvent.h>
#include "types.h"
#include "core/core.h"
#include "devices/nvdevice.h"

namespace skyline::service::nvdrv {
    namespace device {
        namespace nvhost {
            class AsGpu;
        }
    }

    class Driver {
      private:
        const DeviceState &state;

        std::shared_mutex deviceMutex; //!< Protects access to `devices`
        std::unordered_map<FileDescriptor, std::unique_ptr<device::NvDevice>> devices;

        friend device::nvhost::AsGpu; // For channel address space binding

      public:
        Core core; //!< The core global state object of nvdrv that is accessed by devices

        Driver(const DeviceState &state);

        /**
         * @brief Creates a new device as specified by path
         * @param path The /dev path that corresponds to the device
         * @param fd The fd that will be used to refer to the device
         * @param ctx The context to be attached to the device
         */
        NvResult OpenDevice(std::string_view path, FileDescriptor fd, const SessionContext &ctx);

        /**
         * @brief Calls an IOCTL on the device specified by `fd`
         */
        NvResult Ioctl(u32 fd, IoctlDescriptor cmd, span<u8> buffer);

        /**
         * @brief Calls an IOCTL on the device specified by `fd` using the given inline input buffer
         */
        NvResult Ioctl2(u32 fd, IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Calls an IOCTL on the device specified by `fd` using the given inline output buffer
         */
        NvResult Ioctl3(u32 fd, IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineBuffer);

        /**
         * @brief Queries a KEvent for the given `eventId` for the device specified by `fd`
         */
        std::shared_ptr<kernel::type::KEvent> QueryEvent(u32 fd, u32 eventId);

        /**
         * @brief Closes the device specified by `fd`
         */
        void CloseDevice(u32 fd);
    };
}
