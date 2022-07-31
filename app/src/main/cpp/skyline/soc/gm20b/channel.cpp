// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "engines/maxwell_3d.h" //TODO: remove
#include "engines/fermi_2d.h" //TODO: remove
#include "channel.h"

namespace skyline::soc::gm20b {
    ChannelContext::ChannelContext(const DeviceState &state, std::shared_ptr<AddressSpaceContext> pAsCtx, size_t numEntries)
        : asCtx(std::move(pAsCtx)),
          executor(state),
          maxwell3D(std::make_unique<engine::maxwell3d::Maxwell3D>(state, *this, macroState, executor)),
          fermi2D(state, *this, macroState, executor),
          maxwellDma(state, *this, executor),
          keplerCompute(state, *this),
          inline2Memory(*this),
          gpfifo(state, *this, numEntries) {}
}
