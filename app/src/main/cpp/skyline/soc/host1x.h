// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "host1x/syncpoint.h"
#include "host1x/command_fifo.h"

namespace skyline::soc::host1x {
    constexpr static size_t ChannelCount{14}; //!< The number of channels within host1x

    /**
     * @brief An abstraction for the graphics host, this handles DMA on behalf of the CPU when communicating to it's clients alongside handling syncpts
     * @note This is different from the GM20B Host, it serves a similar function and has an interface for accessing host1x syncpts
     */
    class Host1x {
      public:
        SyncpointSet syncpoints;
        std::array<ChannelCommandFifo, ChannelCount> channels;

        Host1x(const DeviceState &state) : channels{util::MakeFilledArray<ChannelCommandFifo, ChannelCount>(state, syncpoints)} {}
    };
}
