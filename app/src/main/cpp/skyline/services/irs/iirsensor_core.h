// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <os.h>
#include <kernel/types/KProcess.h>

namespace skyline::service::irs {
    struct SharedIirCore {
        static constexpr u32 IirSharedMemSize{0x8000};
        std::shared_ptr<kernel::type::KSharedMemory> sharedIirMemory;

        SharedIirCore(const DeviceState &state) : sharedIirMemory(std::make_shared<kernel::type::KSharedMemory>(state, IirSharedMemSize)) {}
    };
}