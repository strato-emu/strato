// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/maxwell_dma.h>
#include "engine.h"

namespace skyline::gpu::interconnect {
    class CommandExecutor;
}

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine {
    /**
    * @brief The Maxwell DMA Engine is used to perform DMA buffer/texture copies directly on the GPU
    */
    class MaxwellDma {
      private:
        host1x::SyncpointSet &syncpoints;
        ChannelContext &channelCtx;
        gpu::interconnect::MaxwellDma interconnect;
        std::vector<u8> copyCache;

        void HandleMethod(u32 method, u32 argument);

        void DmaCopy();

        void HandleSplitCopy(TranslatedAddressRange srcMappings, TranslatedAddressRange dstMappings, size_t srcSize, size_t dstSize, auto copyCallback);

        void CopyPitchToPitch();

        void CopyBlockLinearToPitch();

        void CopyPitchToBlockLinear();

        void LaunchDma();

        void ReleaseSemaphore();

      public:
        /**
         * @url https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/dma-copy/clb0b5.h
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, EngineMethodsEnd> raw;

            template<size_t Offset, typename Type>
            using Register = util::OffsetMember<Offset, Type, u32>;

            Register<0x40, u32> nop;
            Register<0x50, u32> pmTrigger;

            struct Semaphore {
                Address address;
                u32 payload;
            };
            static_assert(sizeof(Semaphore) == 0xC);

            Register<0x90, Semaphore> semaphore;

            struct RenderEnable {
                enum class Mode : u8 {
                    False = 0,
                    True = 1,
                    Conditional = 2,
                    RenderIfEqual = 3,
                    RenderIfNotEqual = 4
                };

                Address address;
                Mode mode : 3;
                u32 _pad_ : 29;
            };
            static_assert(sizeof(RenderEnable) == 0xC);

            Register<0x95, RenderEnable> renderEnable;

            struct PhysMode {
                enum class Target : u8 {
                    LocalFb = 0,
                    CoherentSysmem = 1,
                    NoncoherentSysmem = 2
                };

                Target target : 2;
                u32 _pad_ : 30;
            };

            Register<0x98, PhysMode> srcPhysMode;
            Register<0x99, PhysMode> dstPhysMode;

            struct LaunchDma {
                enum class DataTransferType : u8 {
                    None = 0,
                    Pipelined = 1,
                    NonPipelined = 2
                };

                enum class SemaphoreType : u8 {
                    None = 0,
                    ReleaseOneWordSemaphore = 1,
                    ReleaseFourWordSemaphore = 2
                };

                enum class InterruptType : u8 {
                    None = 0,
                    Blocking = 1,
                    NonBlocking = 2
                };

                enum class MemoryLayout : u8 {
                    BlockLinear = 0,
                    Pitch = 1
                };

                enum class Type : u8 {
                    Virtual = 0,
                    Physical = 1
                };

                enum class SemaphoreReduction : u8 {
                    IMin = 0,
                    IMax = 1,
                    IXor = 2,
                    IAnd = 3,
                    IOr = 4,
                    IAdd = 5,
                    Inc = 6,
                    Dec = 7,
                    FAdd = 10,
                };

                enum class SemaphoreReductionSign : u8 {
                    Signed = 0,
                    Unsigned = 1,
                };

                enum class BypassL2 : u8 {
                    UsePteSetting = 0,
                    ForceVolatile = 1,
                };

                DataTransferType dataTransferType : 2;
                bool flushEnable : 1;
                SemaphoreType semaphoreType : 2;
                InterruptType interruptType : 2;
                MemoryLayout srcMemoryLayout : 1;
                MemoryLayout dstMemoryLayout : 1;
                bool multiLineEnable : 1;
                bool remapEnable : 1;
                bool rmwDisable : 1;
                Type srcType : 1;
                Type dstType : 1;
                SemaphoreReduction semaphoreReduction : 4;
                SemaphoreReductionSign semaphoreReductionSign : 1;
                bool reductionEnable : 1;
                BypassL2 bypassL2 : 1;
                u16 _pad_ : 11;
            };
            static_assert(sizeof(LaunchDma) == 4);

            Register<0xC0, LaunchDma> launchDma;

            Register<0x100, Address> offsetIn;
            Register<0x102, Address> offsetOut;

            Register<0x104, u32> pitchIn;
            Register<0x105, u32> pitchOut;

            Register<0x106, u32> lineLengthIn;
            Register<0x107, u32> lineCount;

            Register<0x1C0, u32> remapConstA;
            Register<0x1C1, u32> remapConstB;

            struct RemapComponents {
                enum class Swizzle : u8 {
                    SrcX = 0,
                    SrcY = 1,
                    SrcZ = 2,
                    SrcW = 3,
                    ConstA = 4,
                    ConstB = 5,
                    NoWrite = 6
                };

                Swizzle dstX : 3;
                u8 _pad0_ : 1;
                Swizzle dstY : 3;
                u8 _pad1_ : 1;
                Swizzle dstZ : 3;
                u8 _pad2_ : 1;
                Swizzle dstW : 3;
                u8 _pad3_ : 1;

                u8 componentSizeMinusOne : 2;
                u8 _pad4_ : 2;
                u8 numSrcComponentsMinusOne : 2;
                u8 _pad5_ : 2;
                u8 numDstComponentsMinusOne : 2;
                u8 _pad6_ : 6;

                u8 ComponentSize() {
                    return componentSizeMinusOne + 1;
                }

                u8 NumSrcComponents() {
                    return numSrcComponentsMinusOne + 1;
                }

                u8 NumDstComponents() {
                    return numDstComponentsMinusOne + 1;
                }
            };
            static_assert(sizeof(RemapComponents) == 0x4);

            Register<0x1C2, RemapComponents> remapComponents;

            struct Surface {
                struct {
                    u8 widthLog2 : 4;
                    u8 heightLog2 : 4;
                    u8 depthLog2 : 4;
                    u8 gobHeight : 4;
                    u16 _pad_;

                    u8 Width() {
                        return static_cast<u8>(1 << widthLog2);
                    }

                    u8 Height() {
                        return static_cast<u8>(1 << heightLog2);
                    }

                    u8 Depth() {
                        return static_cast<u8>(1 << depthLog2);
                    }
                } blockSize;
                u32 width;
                u32 height;
                u32 depth;
                u32 layer;

                struct {
                    u16 x;
                    u16 y;
                } origin;
            };
            static_assert(sizeof(Surface) == 0x18);

            Register<0x1C3, Surface> dstSurface;
            Register<0x1CA, Surface> srcSurface;
        } registers{};
        static_assert(sizeof(Registers) == (EngineMethodsEnd * 0x4));
        #pragma pack(pop)

        MaxwellDma(const DeviceState &state, ChannelContext &channelCtx);

        void CallMethod(u32 method, u32 argument);

        void CallMethodBatchNonInc(u32 method, span<u32> arguments);
    };
}
