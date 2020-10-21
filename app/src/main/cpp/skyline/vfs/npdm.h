// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "backing.h"
#include <kernel/memory.h>

namespace skyline::vfs {
    /**
     * @url https://switchbrew.org/wiki/NPDM
     */
    class NPDM {
      public:
        /**
         * @url https://switchbrew.org/wiki/NPDM#META
         */
        struct __attribute__((packed)) NpdmMeta {
            u32 magic; //!< "META"
            u32 acidSignatureKeyGeneration;
            u32 _unk0_;
            union {
                struct {
                    bool is64Bit : 1;
                    memory::AddressSpaceType type : 2;
                    bool optimizeMemoryAllocation : 1;
                };
                u8 raw{};
            } flags;
            u8 _unk1_;
            u8 mainThreadPriority;
            u8 idealCore;
            u32 _unk2_;
            u32 systemResourceSize; //!< 3.0.0+
            u32 version;
            u32 mainThreadStackSize;
            std::array<char, 0x10> name; //!< "Application"
            std::array<u8, 0x10> productCode;
            u8 _unk3_[0x30];
            u32 aciOffset;
            u32 aciSize;
            u32 acidOffset;
            u32 acidSize;
        } meta;

        static_assert(sizeof(NpdmMeta::flags) == sizeof(u8));
        static_assert(sizeof(NpdmMeta) == 0x80);

      public:
        NPDM();

        NPDM(const std::shared_ptr<vfs::Backing> &backing);
    };
}
