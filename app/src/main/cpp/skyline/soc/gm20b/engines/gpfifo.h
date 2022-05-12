// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "engine.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::soc::gm20b::engine {
    /**
    * @brief The GPFIFO engine handles managing macros and semaphores
    * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/manuals/volta/gv100/dev_pbdma.ref.txt
    */
    class GPFIFO {
      public:
        static constexpr u32 RegisterCount{0x40}; //!< The number of GPFIFO registers

      private:
        /**
         * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/classes/host/clb06f.h#L65
         */
        #pragma pack(push, 1)
        union Registers {
            std::array<u32, RegisterCount> raw;

            template<size_t Offset, typename Type>
            using Register = util::OffsetMember<Offset, Type, u32>;

            struct SetObject {
                u16 nvClass : 16;
                u8 engine : 5;
                u16 _pad_ : 11;
            };
            static_assert(sizeof(SetObject) == 0x4);

            Register<0x0, SetObject> setObject;

            Register<0x1, u32> illegal;
            Register<0x1, u32> nop;

            struct Semaphore {
                enum class Operation : u8 {
                    Acquire = 1,
                    Release = 2,
                    AcqGeq = 4,
                    AcqAnd = 8,
                    Reduction = 16,
                };

                enum class AcquireSwitch : u8 {
                    Disabled = 0,
                    Enabled = 1,
                };

                enum class ReleaseWfi : u8 {
                    En = 0,
                    Dis = 1,
                };

                enum class ReleaseSize : u8 {
                    SixteenBytes = 0,
                    FourBytes = 1,
                };

                enum class Reduction : u8 {
                    Min = 0,
                    Max = 1,
                    Xor = 2,
                    And = 3,
                    Or = 4,
                    Add = 5,
                    Inc = 6,
                    Dec = 7,
                };

                enum class Format : u8 {
                    Signed = 0,
                    Unsigned = 1,
                };

                Address address; // 0x4
                u32 payload; // 0x6

                struct {
                    Operation operation : 5;
                    u8 _pad2_ : 7;
                    AcquireSwitch acquireSwitch : 1;
                    u8 _pad3_ : 7;
                    ReleaseWfi releaseWfi : 1;
                    u8 _pad4_ : 3;
                    ReleaseSize releaseSize : 1;
                    u8 _pad5_ : 2;
                    Reduction reduction : 4;
                    Format format : 1;
                } action; // 0x7
            };
            static_assert(sizeof(Semaphore) == 0x10);

            Register<0x4, Semaphore> semaphore;

            Register<0x8, u32> nonStallInterrupt;
            Register<0x9, u32> fbFlush;

            Register<0xC, u32> memOpC;
            Register<0xD, u32> memOpD;

            Register<0x14, u32> setReference;

            struct Syncpoint {
                enum class Operation : u8 {
                    Wait = 0,
                    Incr = 1,
                };

                enum class WaitSwitch : u8 {
                    Dis = 0,
                    En = 1,
                };

                u32 payload; // 0x1C

                struct {
                    Operation operation : 1;
                    u8 _pad0_ : 3;
                    WaitSwitch waitSwitch : 1; //!< If the PBDMA unit can switch to a different timeslice group (TSG) while waiting on a syncpoint
                    u8 _pad1_ : 3;
                    u16 index : 12;
                    u16 _pad2_ : 12;
                } action; // 0x1D
            };
            static_assert(sizeof(Syncpoint) == 0x8);

            Register<0x1C, Syncpoint> syncpoint;

            struct Wfi {
                enum class Scope : u8 {
                    CurrentScgType = 0,
                    All = 1,
                };

                Scope scope : 1;
                u32 _pad_ : 31;
            };
            static_assert(sizeof(Wfi) == 0x4);

            Register<0x1E, Wfi> wfi;

            Register<0x1F, u32> crcCheck;

            struct Yield {
                enum class Op : u8 {
                    Nop = 0,
                    PbdmaTimeslice = 1,
                    RunlistTimeslice = 2,
                    Tsg = 3,
                };

                Op op : 2;
                u32 _pad_ : 30;
            };
            static_assert(sizeof(Yield) == 0x4);

            Register<0x20, Yield> yield;
        } registers{};
        static_assert(sizeof(Registers) == (RegisterCount * sizeof(u32)));
        #pragma pack(pop)

        host1x::SyncpointSet &syncpoints;
        ChannelContext &channelCtx;

      public:
        GPFIFO(host1x::SyncpointSet &syncpoints, ChannelContext &channelCtx);

        void CallMethod(u32 method, u32 argument);
    };
}
