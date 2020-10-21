// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KThread.h>
#include "npdm.h"

namespace skyline::vfs {
    constexpr u32 MetaMagic = util::MakeMagic<u32>("META");

    NPDM::NPDM() {
        constexpr i8 DefaultPriority{44}; // The default priority of an HOS process
        constexpr i8 DefaultCore{0}; // The default core for an HOS process
        constexpr u64 DefaultStackSize{0x200000}; //!< The default amount of stack: 2 MiB
        constexpr u64 DefaultSystemResourceSize{0x1FE00000}; //!< The amount of memory reserved for system resources, it's the maximum at 510 MiB
        meta = {
            .magic = MetaMagic,
            .flags = {
                {
                    .is64Bit = true,
                    .type = memory::AddressSpaceType::AddressSpace39Bit,
                    .optimizeMemoryAllocation = false,
                }
            },
            .mainThreadPriority = DefaultPriority,
            .idealCore = DefaultCore,
            .mainThreadStackSize = DefaultStackSize,
            .systemResourceSize = DefaultSystemResourceSize,
            .name = "Application",
        };
    }

    NPDM::NPDM(const std::shared_ptr<vfs::Backing> &backing) {
        meta = backing->Read<NpdmMeta>();
        if(meta.magic != MetaMagic)
            throw exception("NPDM Meta Magic isn't correct: 0x{:X} (\"META\" = 0x{:X})", meta.magic, MetaMagic);
        if (!constant::HosPriority.Valid(meta.mainThreadPriority))
            throw exception("NPDM Main Thread Priority isn't valid: {}", meta.mainThreadStackSize);
        if (meta.idealCore >= constant::CoreCount)
            throw exception("NPDM Ideal Core isn't valid: {}", meta.idealCore);
        if (!util::PageAligned(meta.mainThreadStackSize))
            throw exception("NPDM Main Thread Stack isn't page aligned: 0x{:X}", meta.mainThreadStackSize);
    }
}
