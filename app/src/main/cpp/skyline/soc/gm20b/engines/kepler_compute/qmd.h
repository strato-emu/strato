// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <common.h>

namespace skyline::soc::gm20b::engine::kepler_compute {
    #pragma pack(push, 1)

    /**
     * @brief Holds the 'Compute Queue Metadata' structure which encapsulates the state needed to execute a compute task
     * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/compute_qmd.h
     */
    struct QMD {
        static constexpr size_t ConstantBufferCount{8};

        enum class DependentQmdType : u32 {
            Queue = 0,
            Grid = 1
        };

        enum class ReleaseMemBarType : u32 {
            FeNone = 0,
            FeSysmem = 1
        };

        enum class CwdMemBarType : u32 {
            L1None = 0,
            L1SysmemBar = 1,
            L1MemBar = 2
        };

        enum class Fp32NanBehaviour : u32 {
            Legacy = 0,
            Fp64Compatible = 1
        };

        enum class Fp32F2iNanBehavior : u32 {
            PassZero = 0,
            PassIndefinite = 1
        };

        enum class ApiVisibleCallLimit : u32 {
            ThirtyTwo = 0,
            NoCheck = 1
        };

        enum class SharedMemoryBankMapping : u32 {
            FourBytesPerBank = 0,
            EightBytesPerBank = 1
        };

        enum class SamplerIndex : u32 {
            Independently = 0,
            ViaHeaderIndex = 1
        };

        enum class Fp32NarrowInstruction : u32 {
            KeepDenorms = 0,
            FlushDenorms = 1
        };

        enum class L1Configuration : u32 {
            DirectlyAddressableMemorySize16Kb = 0,
            DirectlyAddressableMemorySize32Kb = 1,
            DirectlyAddressableMemorySize48Kb = 2
        };

        enum class ReductionOp : u32 {
            RedAdd = 0,
            RedMin = 1,
            RedMax = 2,
            RedInc = 3,
            RedDec = 4,
            RedAnd = 5,
            RedOr = 6,
            RedXor = 7
        };

        enum class ReductionFormat : u32 {
            Unsigned32 = 0,
            Signed32 = 1
        };

        enum class StructureSize : u32 {
            FourWords = 0,
            OneWord = 1
        };

        u32 outerPut : 31;
        u32 outerOverflow : 1;
        u32 outerGet : 31;
        u32 outerStickyOverflow : 1;

        u32 innerGet : 31;
        u32 innerOverflow : 1;
        u32 innerPut : 31;
        u32 innerStickyOverflow : 1;

        u32 qmdReservedAA;

        u32 dependentQmdPointer;

        u32 qmdGroupId : 6;

        u32 smGlobalCachingEnable : 1;

        u32 runCtaInOneSmPartition : 1;

        u32 isQueue : 1;

        u32 addToHeadOfQmdGroupLinkedList : 1;

        u32 semaphoreReleaseEnable0 : 1;
        u32 semaphoreReleaseEnable1 : 1;

        u32 requireSchedulingPcas : 1;
        u32 dependentQmdScheduleEnable : 1;
        DependentQmdType dependentQmdType : 1;
        u32 dependentQmdFieldCopy : 1;

        u32 qmdReservedB : 16;

        u32 circularQueueSize : 25;

        u32 qmdReservedC : 1;

        u32 invalidateTextureHeaderCache : 1;
        u32 invalidateTextureSamplerCache : 1;
        u32 invalidateTextureDataCache : 1;
        u32 invalidateShaderDataCache : 1;
        u32 invalidateInstructionCache : 1;
        u32 invalidateShaderConstantCache : 1;

        u32 programOffset;

        u32 circularQueueAddrLower;
        u32 circularQueueAddrUpper : 8;

        u32 qmdReservedD : 8;

        u32 circularQueueEntrySize : 16;

        u32 cwdReferenceCountId : 6;
        u32 cwdReferenceCountDeltaMinusOne : 8;

        ReleaseMemBarType releaseMembarType : 1;

        u32 cwdReferenceCountIncrEnable : 1;
        CwdMemBarType cwdMembarType : 2;

        u32 sequentiallyRunCtas : 1;

        u32 cwdReferenceCountDecrEnable : 1;

        u32 throttled : 1;

        u32 _pad0_ : 3;

        Fp32NanBehaviour fp32NanBehavior : 1;

        Fp32F2iNanBehavior fp32F2iNanBehavior : 1;

        ApiVisibleCallLimit apiVisibleCallLimit : 1;

        SharedMemoryBankMapping sharedMemoryBankMapping : 1;

        u32 _pad1_ : 2;

        SamplerIndex samplerIndex : 1;

        Fp32NarrowInstruction fp32NarrowInstruction : 1;

        u32 ctaRasterWidth;
        u32 ctaRasterHeight : 16;
        u32 ctaRasterDepth : 16;

        u32 ctaRasterWidthResume;
        u32 ctaRasterHeightResume : 16;
        u32 ctaRasterDepthResume : 16;

        u32 queueEntriesPerCtaMinusOne : 7;

        u32 _pad2_ : 3;

        u32 coalesceWaitingPeriod : 8;

        u32 _pad3_ : 14;

        u32 sharedMemorySize : 18;

        u32 qmdReservedG : 14;

        u32 qmdVersion : 4;
        u32 qmdMajorVersion : 4;

        u32 qmdReservedH : 8;

        u32 ctaThreadDimension0 : 16;
        u32 ctaThreadDimension1 : 16;
        u32 ctaThreadDimension2 : 16;

        u32 constantBufferValid : 8;

        u32 qmdReservedI : 21;

        L1Configuration l1Configuration : 3;

        u32 smDisableMaskLower;
        u32 smDisableMaskUpper;

        struct {
            u32 addressLower;
            u32 addressUpper : 8;
            u32 qmdReservedJL : 8;
            u32 _pad4_ : 4;
            ReductionOp reductionOp : 3;
            u32 qmdReservedKM : 1;
            ReductionFormat reductionFormat : 2;
            u32 reductionEnable : 1;
            u32 _pad5_ : 4;
            StructureSize structureSize : 1;
            u32 payload;
        } release[2];

        struct {
            u32 addrLower;
            u32 addrUpper : 8;
            u32 reservedAddr : 6;
            u32 invalidate : 1;
            u32 size : 17;

            u64 Address() const {
                return (static_cast<u64>(addrUpper) << 32) | addrLower;
            }
        } constantBuffer[ConstantBufferCount];

        u32 shaderLocalMemoryLowSize : 24;

        u32 qmdReservedN : 3;

        u32 barrierCount : 5;
        u32 shaderLocalMemoryHighSize : 24;
        u32 registerCount : 8;
        u32 shaderLocalMemoryCrsSize : 24;

        u32 sassVersion : 8;

        u32 hwOnlyInnerGet : 31;
        u32 hwOnlyRequireSchedulingPcas : 1;
        u32 hwOnlyInnerPut : 31;
        u32 hwOnlyScgType : 1;
        u32 hwOnlySpanListHeadIndex : 30;

        u32 qmdReservedQ : 1;

        u32 hwOnlySpanListHeadIndexValid : 1;
        u32 hwOnlySkedNextQmdPointer;

        u32 qmdSpareEFGHIJKLMN[10];

        u32 debugIdLower;
        u32 debugIdUpper;
    };
    static_assert(sizeof(QMD) == 0x100);
    #pragma pack(pop)

}