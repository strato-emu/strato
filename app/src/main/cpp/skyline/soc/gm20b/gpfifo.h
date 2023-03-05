// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/circular_queue.h>
#include <soc/gm20b/macro/macro_state.h>
#include "engines/gpfifo.h"

namespace skyline::soc::gm20b {
    struct ChannelContext;

    /**
     * @brief Mapping of subchannel names to their corresponding subchannel IDs
     */
    enum class SubchannelId : u8 {
        ThreeD = 0,
        Compute = 1,
        Inline2Mem = 2,
        TwoD = 3,
        Copy = 4,
        Software0 = 5,
        Software1 = 6,
        Software2 = 7,
    };

    /**
     * @brief A GPFIFO entry as submitted through 'SubmitGpfifo'
     * @url https://nvidia.github.io/open-gpu-doc/manuals/volta/gv100/dev_pbdma.ref.txt
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/classes/host/clb06f.h#L155
     */
    struct GpEntry {
        enum class Fetch : u8 {
            Unconditional = 0,
            Conditional = 1,
        };

        union {
            u32 entry0{};

            struct {
                Fetch fetch : 1;
                u8 _pad_ : 1;
                u32 get : 30;
            };
        };

        enum class Opcode : u8 {
            Nop = 0,
            Illegal = 1,
            Crc = 2,
            PbCrc = 3,
        };

        enum class Priv : u8 {
            User = 0,
            Kernel = 1,
        };

        enum class Level : u8 {
            Main = 0,
            Subroutine = 1,
        };

        enum class Sync : u8 {
            Proceed = 0,
            Wait = 1,
        };

        union {
            u32 entry1{};

            struct {
                union {
                    u8 getHi;
                    Opcode opcode;
                };

                Priv priv : 1;
                Level level : 1;
                u32 size : 21;
                Sync sync : 1;
            };
        };

        constexpr GpEntry(u64 gpuAddress, u32 pSize) {
            getHi = static_cast<u8>(gpuAddress >> 32);
            get = static_cast<u32>(gpuAddress >> 2);
            size = pSize;
        }

        constexpr u64 Address() const {
            return (static_cast<u64>(getHi) << 32) | (static_cast<u64>(get) << 2);
        }
    };
    static_assert(sizeof(GpEntry) == sizeof(u64));

    /**
     * @brief The ChannelGpfifo class handles creating pushbuffers from GP entries and then processing them for a single channel
     * @note A single ChannelGpfifo thread exists per channel, allowing them to run asynchronously
     * @note This class doesn't perfectly map to any particular hardware component on the X1, it does a mix of the GPU Host PBDMA and handling the GPFIFO entries
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/manuals/volta/gv100/dev_pbdma.ref.txt#L62
     */
    class ChannelGpfifo {
      private:
        const DeviceState &state;
        ChannelContext &channelCtx;
        engine::GPFIFO gpfifoEngine; //!< The engine for processing GPFIFO method calls
        CircularQueue<GpEntry> gpEntries;
        std::vector<u32> pushBufferData; //!< Persistent vector storing pushbuffer data to avoid constant reallocations
        bool skipDirtyFlushes{}; //!< If GPU flushing should be skipped when fetching pushbuffer contents

        /**
         * @brief Holds the required state in order to resume a method started from one call to `Process` in another
         * @note This is needed as games (especially OpenGL ones) can split method entries over multiple GpEntries
         */
        struct MethodResumeState {
            u32 remaining; //!< The number of entries left to handle until the method is finished
            u32 address; //!< The method address in the GPU block specified by `subchannel` that is the target of the command
            SubchannelId subChannel;

            /**
             * @brief This is a simplified version of the full method type enum
             */
            enum class State : u8 {
                NonInc,
                Inc,
                OneInc //!< Will be switched to NonInc after the first call
            } state; //!< The type of method to resume
        } resumeState{};

        std::thread thread; //!< The thread that manages processing of pushbuffers

        /**
         * @brief Sends a method call to the appropriate subchannel and handles macro and GPFIFO methods
         */
        void SendFull(u32 method, GpfifoArgument argument, SubchannelId subchannel, bool lastCall);

        /**
         * @brief Sends a method call to the appropriate subchannel, macro and GPFIFO methods are not handled
         */
        void SendPure(u32 method, u32 argument, SubchannelId subchannel);

        /**
         * @brief Sends a batch of method calls all directed at the same method to the appropriate subchannel, macro and GPFIFO methods are not handled
         */
        void SendPureBatchNonInc(u32 method, span<u32> arguments, SubchannelId subChannel);

        /**
         * @brief Processes the pushbuffer contained within the given GpEntry, calling methods as needed
         */
        void Process(GpEntry gpEntry);

        /**
         * @brief Executes all pending entries in the FIFO and polls for more
         */
        void Run();

      public:
        /**
         * @param numEntries The number of gpEntries to allocate space for in the FIFO
         */
        ChannelGpfifo(const DeviceState &state, ChannelContext &channelCtx, size_t numEntries);

        ~ChannelGpfifo();

        /**
         * @brief Pushes a list of entries to the FIFO, these commands will be executed on calls to 'Process'
         */
        void Push(span<GpEntry> entries);

        /**
         * @brief Pushes a single entry to the FIFO, these commands will be executed on calls to 'Process'
         */
        void Push(GpEntry entries);
    };
}
