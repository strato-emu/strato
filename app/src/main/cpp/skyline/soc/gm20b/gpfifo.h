// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/circular_queue.h>
#include "engines/gpfifo.h"

namespace skyline::soc::gm20b {
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
            u32 entry0;

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
            u32 entry1;

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

        constexpr u64 Address() const {
            return (static_cast<u64>(getHi) << 32) | (static_cast<u64>(get) << 2);
        }
    };
    static_assert(sizeof(GpEntry) == sizeof(u64));

    /**
     * @brief The GPFIFO class handles creating pushbuffers from GP entries and then processing them
     * @note This class doesn't perfectly map to any particular hardware component on the X1, it does a mix of the GPU Host PBDMA (With  and handling the GPFIFO entries
     * @url https://github.com/NVIDIA/open-gpu-doc/blob/ab27fc22db5de0d02a4cabe08e555663b62db4d4/manuals/volta/gv100/dev_pbdma.ref.txt#L62
     */
    class GPFIFO {
        const DeviceState &state;
        engine::GPFIFO gpfifoEngine; //!< The engine for processing GPFIFO method calls
        std::array<engine::Engine*, 8> subchannels;
        std::optional<CircularQueue<GpEntry>> pushBuffers;
        std::thread thread; //!< The thread that manages processing of pushbuffers
        std::vector<u32> pushBufferData; //!< Persistent vector storing pushbuffer data to avoid constant reallocations

        /**
         * @brief Sends a method call to the GPU hardware
         */
        void Send(u32 method, u32 argument, u32 subchannel, bool lastCall);

        /**
         * @brief Processes the pushbuffer contained within the given GpEntry, calling methods as needed
         */
        void Process(GpEntry gpEntry);

      public:
        GPFIFO(const DeviceState &state) : state(state), gpfifoEngine(state) {}

        ~GPFIFO();

        /**
         * @param numBuffers The amount of push-buffers to allocate in the circular buffer
         */
        void Initialize(size_t numBuffers);

        /**
         * @brief Executes all pending entries in the FIFO
         */
        void Run();

        /**
         * @brief Pushes a list of entries to the FIFO, these commands will be executed on calls to 'Step'
         */
        void Push(span<GpEntry> entries);
    };
}
