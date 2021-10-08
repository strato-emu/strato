// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/ipc.h>
#include <kernel/types/KEvent.h>
#include <services/common/result.h>
#include <services/nvdrv/types.h>
#include <services/nvdrv/core/core.h>

#include "deserialisation/types.h"

namespace skyline::service::nvdrv {
    class Driver;
}

namespace skyline::service::nvdrv::device {
    using namespace kernel;
    using namespace deserialisation;

    /**
     * @brief NvDevice is the base class that all /dev/nv* devices inherit from
     */
    class NvDevice {
      private:
        std::string name; //!< The name of the device

      protected:
        const DeviceState &state;
        Driver &driver;
        Core &core;
        SessionContext ctx;

      public:
        NvDevice(const DeviceState &state, Driver &driver, Core &core, const SessionContext &ctx);

        virtual ~NvDevice() = default;

        /**
         * @return The name of the class
         * @note The lifetime of the returned string is tied to that of the class
         */
        const std::string &GetName();

        virtual PosixResult Ioctl(IoctlDescriptor cmd, span<u8> buffer) = 0;

        virtual PosixResult Ioctl2(IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineOutput) {
            return PosixResult::InappropriateIoctlForDevice;
        }

        virtual PosixResult Ioctl3(IoctlDescriptor cmd, span<u8> buffer, span<u8> inlineInput) {
            return PosixResult::InappropriateIoctlForDevice;
        }

        virtual std::shared_ptr<kernel::type::KEvent> QueryEvent(u32 eventId) {
            return nullptr;
        }
    };
}
