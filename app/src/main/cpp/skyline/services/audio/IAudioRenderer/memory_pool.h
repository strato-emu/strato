// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::audio::IAudioRenderer {
    /**
     * @brief This enumerates various states a memory pool can be in
     */
    enum class MemoryPoolState : u32 {
        Invalid = 0, //!< The memory pool is invalid
        Unknown = 1, //!< The memory pool is in an unknown state
        RequestDetach = 2, //!< The memory pool should be detached from
        Detached = 3, //!< The memory pool has been detached from
        RequestAttach = 4, //!< The memory pool should be attached to
        Attached = 5, //!< The memory pool has been attached to
        Released = 6 //!< The memory pool has been released
    };

    /**
     * @brief This is in input containing information about a memory pool for use by the dsp
     */
    struct MemoryPoolIn {
        u64 address; //!< The address of the memory pool
        u64 size; //!< The size of the memory pool
        MemoryPoolState state; //!< The state that is requested for the memory pool
        u32 _unk0_;
        u64 _unk1_;
    };
    static_assert(sizeof(MemoryPoolIn) == 0x20);

    /**
     * @brief This is returned to inform the guest of the state of a memory pool
     */
    struct MemoryPoolOut {
        MemoryPoolState state = MemoryPoolState::Detached; //!< The state of the memory pool
        u32 _unk0_;
        u64 _unk1_;
    };
    static_assert(sizeof(MemoryPoolOut) == 0x10);

    /**
    * @brief The MemoryPool class stores the state of a memory pool
    */
    class MemoryPool {
      public:
        MemoryPoolOut output{}; //!< The current output state

        /**
         * @brief Processes the input memory pool data from the guest and sets internal data based off it
         * @param input The input data struct from guest
         */
        void ProcessInput(const MemoryPoolIn &input);
    };
}
