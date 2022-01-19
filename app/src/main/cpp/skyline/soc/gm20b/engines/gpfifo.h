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

            enum class SemaphoreOperation : u8 {
                Acquire = 1,
                Release = 2,
                AcqGeq = 4,
                AcqAnd = 8,
                Reduction = 16,
            };

            enum class SemaphoreAcquireSwitch : u8 {
                Disabled = 0,
                Enabled = 1,
            };

            enum class SemaphoreReleaseWfi : u8 {
                En = 0,
                Dis = 1,
            };

            enum class SemaphoreReleaseSize : u8 {
                SixteenBytes = 0,
                FourBytes = 1,
            };

            enum class SemaphoreReduction : u8 {
                Min = 0,
                Max = 1,
                Xor = 2,
                And = 3,
                Or = 4,
                Add = 5,
                Inc = 6,
                Dec = 7,
            };

            enum class SemaphoreFormat : u8 {
                Signed = 0,
                Unsigned = 1,
            };

            enum class MemOpTlbInvalidatePdb : u8 {
                One = 0,
                All = 1,
            };

            enum class SyncpointOperation : u8 {
                Wait = 0,
                Incr = 1,
            };

            enum class SyncpointWaitSwitch : u8 {
                Dis = 0,
                En = 1,
            };

            enum class WfiScope : u8 {
                CurrentScgType = 0,
                All = 1,
            };

            enum class YieldOp : u8 {
                Nop = 0,
                PbdmaTimeslice = 1,
                RunlistTimeslice = 2,
                Tsg = 3,
            };

            struct {
                struct {
                    u16 nvClass : 16;
                    u8 engine : 5;
                    u16 _pad_ : 11;
                } setObject; // 0x0

                u32 illegal; // 0x1
                u32 nop; // 0x2
                u32 _pad0_; // 0x3

                struct {
                    struct {
                        u32 offsetUpper : 8;
                        u32 _pad0_ : 24;
                    }; // 0x4

                    struct {
                        u8 _pad1_ : 2;
                        u32 offsetLower : 30;
                    }; // 0x5

                    u32 payload; // 0x6

                    struct {
                        SemaphoreOperation operation : 5;
                        u8 _pad2_ : 7;
                        SemaphoreAcquireSwitch acquireSwitch : 1;
                        u8 _pad3_ : 7;
                        SemaphoreReleaseWfi releaseWfi : 1;
                        u8 _pad4_ : 3;
                        SemaphoreReleaseSize releaseSize : 1;
                        u8 _pad5_ : 2;
                        SemaphoreReduction reduction : 4;
                        SemaphoreFormat format : 1;
                    }; // 0x7
                } semaphore;

                u32 nonStallInterrupt; // 0x8
                u32 fbFlush; // 0x9
                u32 _pad1_[2]; // 0xA
                u32 memOpC; // 0xC
                u32 memOpD; // 0xD
                u32 _pad2_[6]; // 0xE
                u32 setReference; // 0x14
                u32 _pad3_[7]; // 0x15

                struct {
                    u32 payload; // 0x1C

                    struct {
                        SyncpointOperation operation : 1;
                        u8 _pad0_ : 3;
                        SyncpointWaitSwitch waitSwitch : 1; //!< If the PBDMA unit can switch to a different timeslice group (TSG) while waiting on a syncpoint
                        u8 _pad1_ : 3;
                        u16 index : 12;
                        u16 _pad2_ : 12;
                    } action; // 0x1D
                } syncpoint;

                struct {
                    WfiScope scope : 1;
                    u32 _pad_ : 31;
                } wfi; // 0x1E

                u32 crcCheck; // 0x1F

                struct {
                    YieldOp op : 2;
                    u32 _pad_ : 30;
                } yield; // 0x20
            };
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
