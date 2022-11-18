// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2018-2020 fincs (https://github.com/devkitPro/deko3d)

#pragma once

#include <gpu/interconnect/kepler_compute/kepler_compute.h>
#include "engine.h"
#include "inline2memory.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine {
    /**
    * @brief The Kepler Compute Engine is used to execute compute jobs on the GPU
    */
    class KeplerCompute {
      private:
        host1x::SyncpointSet &syncpoints;
        ChannelContext &channelCtx;
        Inline2MemoryBackend i2m;
        gpu::interconnect::DirtyManager dirtyManager;
        gpu::interconnect::kepler_compute::KeplerCompute interconnect;

        void HandleMethod(u32 method, u32 argument);

      public:
        /**
         * @url https://github.com/devkitPro/deko3d/blob/master/source/maxwell/engine_compute.def
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, EngineMethodsEnd> raw;

            template<size_t Offset, typename Type>
            using Register = util::OffsetMember<Offset, Type, u32>;

            Register<0x44, u32> waitForIdle;
            Register<0x60, Inline2MemoryBackend::RegisterState> i2m;
            Register<0x85, u32> setShaderSharedMemoryWindow;

            struct InvalidateShaderCaches {
                bool instruction : 1;
                bool locks : 1;
                bool flushData : 1;
                u8 _pad0_ : 1;
                bool data : 1;
                u8 _pad1_ : 7;
                bool constant : 1;
                u32 _pad2_ : 19;
            };
            static_assert(sizeof(InvalidateShaderCaches) == 0x4);

            Register<0x87, InvalidateShaderCaches> invalidateShaderCaches;

            struct SendPcas {
                u32 qmdAddressShifted8;
                u32 from : 24;
                u8 delta;

                u64 QmdAddress() const {
                    return static_cast<u64>(qmdAddressShifted8) << 8;
                }
            };
            static_assert(sizeof(SendPcas) == 0x8);

            Register<0xAD, SendPcas> sendPcas;

            struct SendSignalingPcasB {
                bool invalidate : 1;
                bool schedule : 1;
                u32 _pad_ : 30;
            };
            static_assert(sizeof(SendSignalingPcasB) == 0x4);

            Register<0xAF, SendSignalingPcasB> sendSignalingPcasB;

            struct ShaderLocalMemory {
                u8 sizeUpper;
                u32 _pad0_ : 24;
                u32 sizeLower;
                u16 maxSmCount : 9;
                u32 _pad1_ : 23;
            };
            static_assert(sizeof(ShaderLocalMemory) == 0xC);

            Register<0xB9, ShaderLocalMemory> shaderLocalMemoryNonThrottled;
            Register<0xBC, ShaderLocalMemory> shaderLocalMemoryThrottled;

            struct SpaVersion {
                u8 minor;
                u8 major;
                u16 _pad_;
            };
            static_assert(sizeof(SpaVersion) == 0x4);

            Register<0xC4, SpaVersion> spaVersion;

            Register<0x1DF, u32> shaderLocalMemoryWindow;
            Register<0x1E4, Address> shaderLocalMemory;

            Register<0x54A, u32> shaderExceptions;

            Register<0x557, TexSamplerPool> texSamplerPool;
            Register<0x55D, TexHeaderPool> texHeaderPool;

            Register<0x582, Address> programRegion;

            struct ReportSemaphore {
                enum class Op : u8 {
                    Release = 0,
                    Trap = 3
                };

                enum class ReductionOp : u8 {
                    Add = 0,
                    Min = 1,
                    Max = 2,
                    Inc = 3,
                    Dec = 4,
                    And = 5,
                    Or = 6,
                    Xor = 7
                };

                enum class Format : u8 {
                    Unsigned32 = 0,
                    Signed32 = 1
                };

                enum class StructureSize : u8 {
                    FourWords = 0,
                    OneWord = 1
                };

                Address offset;
                u32 payload;
                struct {
                    Op op : 2;
                    bool flushDisable : 1;
                    bool reductionEnable : 1;
                    u8 _pad0_ : 5;
                    ReductionOp reductionOp : 3;
                    u8 _pad1_ : 5;
                    Format format : 2;
                    u8 _pad2_ : 1;
                    bool awakenEnable : 1;
                    u8 _pad3_ : 7;
                    StructureSize structureSize : 1;
                    u8 _pad4_ : 3;
                } action;
            };
            static_assert(sizeof(ReportSemaphore) == 0x10);

            Register<0x6C0, ReportSemaphore> reportSemaphore;

            Register<0x982, BindlessTexture> bindlessTexture;
        } registers{};
        static_assert(sizeof(Registers) == (EngineMethodsEnd * 0x4));
        #pragma pack(pop)

        KeplerCompute(const DeviceState &state, ChannelContext &channelCtx);

        void CallMethod(u32 method, u32 argument);

        void CallMethodBatchNonInc(u32 method, span<u32> arguments);
    };
}
