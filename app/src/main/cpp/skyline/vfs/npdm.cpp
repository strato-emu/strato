// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KThread.h>
#include "npdm.h"

namespace skyline::vfs {
    constexpr u32 MetaMagic{util::MakeMagic<u32>("META")};

    NPDM::NPDM() {
        constexpr i8 DefaultPriority{44}; // The default priority of an HOS process
        constexpr i8 DefaultCore{0}; // The default core for an HOS process
        constexpr u64 DefaultStackSize{0x200000}; //!< The default amount of stack: 2 MiB
        constexpr u64 DefaultSystemResourceSize{0x1FE00000}; //!< The amount of memory reserved for system resources, it's the maximum at 510 MiB
        meta = NpdmMeta{
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
        aci0 = {
            .magic = MetaMagic,
        };
        threadInfo = {
            .coreMask = 0b0111,
            .priority = {0, 59},
        };
    }

    NPDM::NPDM(const std::shared_ptr<vfs::Backing> &backing) {
        meta = backing->Read<NpdmMeta>();
        if (meta.magic != MetaMagic)
            throw exception("NPDM Meta Magic isn't correct: 0x{:X} (\"META\" = 0x{:X})", meta.magic, MetaMagic);
        if (!util::IsPageAligned(meta.mainThreadStackSize))
            throw exception("NPDM Main Thread Stack isn't page aligned: 0x{:X}", meta.mainThreadStackSize);

        aci0 = meta.aci0.Read<NpdmAci0>(backing);
        constexpr u32 Aci0Magic{util::MakeMagic<u32>("ACI0")};
        if (aci0.magic != Aci0Magic)
            throw exception("NPDM ACI0 Magic isn't correct: 0x{:X} (\"ACI0\" = 0x{:X})", aci0.magic, Aci0Magic);

        std::vector<NpdmKernelCapability> capabilities(aci0.kernelCapability.size / sizeof(NpdmKernelCapability));
        backing->Read(span(capabilities), meta.aci0.offset + aci0.kernelCapability.offset);
        for (auto capability : capabilities) {
            auto trailingOnes{std::countr_zero(~capability.raw)};
            switch (trailingOnes) {
                case 3:
                    threadInfo.priority = kernel::Priority{static_cast<i8>(capability.threadInfo.highestPriority),
                                                           static_cast<i8>(capability.threadInfo.lowestPriority)};

                    threadInfo.coreMask = {};
                    for (u8 core{capability.threadInfo.minCoreId}; core <= capability.threadInfo.maxCoreId; core++)
                        threadInfo.coreMask.set(core);
                    break;

                case 14:
                    kernelVersion.minorVersion = capability.kernelVersion.minorVersion;
                    kernelVersion.majorVersion = capability.kernelVersion.majorVersion;
                    break;

                default:
                    break;
            }
        }

        if (!threadInfo.priority.Valid(meta.mainThreadPriority))
            throw exception("NPDM Main Thread Priority isn't valid: {} ({} - {})", meta.mainThreadPriority, threadInfo.priority.min, threadInfo.priority.max);
        if (!threadInfo.coreMask.test(meta.idealCore))
            throw exception("NPDM Ideal Core isn't valid: {} ({})", meta.idealCore, threadInfo.coreMask);

        Logger::InfoNoPrefix("NPDM Metadata:\nTitle: ID: {:X}, Version: {}\nMain Thread: Priority: {}, Stack Size: 0x{:X}\nScheduler: Ideal Core: {}, Core Mask: {}, Priority: {} - {}\nKernel Version: v{}.{}", aci0.programId, meta.version, meta.mainThreadPriority, meta.mainThreadStackSize, meta.idealCore, threadInfo.coreMask, threadInfo.priority.min, threadInfo.priority.max, kernelVersion.majorVersion, kernelVersion.minorVersion);
    }
}
