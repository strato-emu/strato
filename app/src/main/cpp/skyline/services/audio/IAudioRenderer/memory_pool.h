// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::audio::IAudioRenderer {
    /**
     * @brief This enumerates various states a memory pool can be in
     */
    enum class MemoryPoolState : u32 {
        Invalid = 0,
        Unknown = 1,
        RequestDetach = 2,
        Detached = 3,
        RequestAttach = 4,
        Attached = 5,
        Released = 6,
    };

    /**
     * @brief This is in input containing information about a memory pool for use by the dsp
     */
    struct MemoryPoolIn {
        u64 address;
        u64 size;
        MemoryPoolState state;
        u32 _unk0_;
        u64 _unk1_;
    };
    static_assert(sizeof(MemoryPoolIn) == 0x20);

    /**
     * @brief This is returned to inform the guest of the state of a memory pool
     */
    struct MemoryPoolOut {
        MemoryPoolState state = MemoryPoolState::Detached;
        u32 _unk0_;
        u64 _unk1_;
    };
    static_assert(sizeof(MemoryPoolOut) == 0x10);

    /**
    * @brief The MemoryPool class stores the state of a memory pool
    */
    class MemoryPool {
      public:
        MemoryPoolOut output{};

        /**
         * @brief Processes the input memory pool data from the guest and sets internal data based off it
         * @param input The input data struct from guest
         */
        void ProcessInput(const MemoryPoolIn &input);
    };
}
