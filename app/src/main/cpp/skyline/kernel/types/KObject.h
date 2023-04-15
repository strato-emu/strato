// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::kernel::type {
    /**
     * @brief These types are used to perform runtime evaluation of a kernel object's type when converting from base class
     */
    enum class KType {
        KThread,
        KProcess,
        KSharedMemory,
        KTransferMemory,
        KSession,
        KEvent,
    };

    /**
     * @brief A base class that all Kernel objects have to derive from
     */
    class KObject {
      public:
        const DeviceState &state;
        KType objectType;

        KObject(const DeviceState &state, KType objectType) : state(state), objectType(objectType) {}
    };
}
