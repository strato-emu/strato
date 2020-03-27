// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::kernel::type {
    /**
     * @brief These types are used to perform runtime evaluation of a kernel object's type when converting from base class
     */
    enum class KType {
        KThread, KProcess, KSharedMemory, KTransferMemory, KPrivateMemory, KSession, KEvent
    };

    /**
     * @brief A base class that all Kernel objects have to derive from
     */
    class KObject {
      public:
        const DeviceState &state; //!< The state of the device
        KType objectType; //!< The type of this object

        /**
         * @param state The state of the device
         * @param objectType The type of the object
         */
        KObject(const DeviceState &state, KType objectType) : state(state), objectType(objectType) {}
    };
}
