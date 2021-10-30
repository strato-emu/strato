// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <common/circular_queue.h>
#include "syncpoint.h"
#include "classes/class.h"
#include "classes/host1x.h"
#include "classes/nvdec.h"
#include "classes/vic.h"
#include "tegra_host_interface.h"

namespace skyline::soc::host1x {
    /**
     * @brief Represents the command FIFO block of the Host1x controller, with one per each channel allowing them to run asynchronously
     * @note A "gather" is equivalent to a GpEntry except we don't need to emulate them directly as they will always be contiguous across CPU memory, hence a regular span is sufficient
     */
    class ChannelCommandFifo {
      private:
        const DeviceState &state;

        static constexpr size_t GatherQueueSize{0x1000}; //!< Maximum size of the gather queue, this value is arbritary
        CircularQueue<span<u32>> gatherQueue;
        std::thread thread; //!< The thread that manages processing of pushbuffers within gathers
        std::mutex threadStartMutex; //!< Protects the thread from being started multiple times

        Host1xClass host1XClass; //!< The internal Host1x class, used for performing syncpoint waits and other general operations
        TegraHostInterface<NvDecClass> nvDecClass; //!< The THI wrapped NVDEC class for video decoding
        TegraHostInterface<VicClass> vicClass; //!< The THI wrapped VIC class for acceleration of image operations

        /**
         * @brief Sends a method call to the target class
         */
        void Send(ClassId targetClass, u32 method, u32 argument);

        /**
         * @brief Processes the pushbuffer contained within the given gather, calling methods as needed
         */
        void Process(span<u32> gather);

        /**
         * @brief Executes all pending gathers in the FIFO and polls for more
         */
        void Run();

      public:
        ChannelCommandFifo(const DeviceState &state, SyncpointSet &syncpoints);

        ~ChannelCommandFifo();

        /**
         * @brief Starts the pushbuffer processing thread if it hasn't already been started
         */
        void Start();

        /**
         * @brief Pushes a single gather into the fifo to be processed asynchronously
         */
        void Push(span<u32> gather);
    };
}