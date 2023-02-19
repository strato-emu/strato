// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/ryujinx/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <soc/gm20b/gmmu.h>

namespace skyline::gpu {
    class GPU;
}

namespace skyline::soc::gm20b {
    struct ChannelContext;
}

namespace skyline::gpu::interconnect {
    class CommandExecutor;

    /**
     * @brief Handles translating I2M operations to Vulkan
     */
    class Inline2Memory {
      private:
        using IOVA = soc::gm20b::IOVA;

        GPU &gpu;
        soc::gm20b::ChannelContext &channelCtx;
        gpu::interconnect::CommandExecutor &executor;

        void UploadSingleMapping(span<u8> dst, span<u8> src);

      public:
        Inline2Memory(GPU &gpu, soc::gm20b::ChannelContext &channelCtx);

        void Upload(IOVA dst, span<u8> src);
    };
}
