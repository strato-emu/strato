// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/ryujinx/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/buffer_manager.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/channel.h>
#include "inline2memory.h"

namespace skyline::gpu::interconnect {
    using IOVA = soc::gm20b::IOVA;

    Inline2Memory::Inline2Memory(GPU &gpu, soc::gm20b::ChannelContext &channelCtx)
        : gpu{gpu},
          channelCtx{channelCtx},
          executor{channelCtx.executor} {}

    void Inline2Memory::Upload(IOVA dst, span<u32> src) {
        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(dst, src.size_bytes())};

        if (dstMappings.size() > 1)
            Logger::Warn("Split mapping are unsupported for DMA copies");

        auto dstBuf{gpu.buffer.FindOrCreate(dstMappings.front(), executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
            executor.AttachLockedBuffer(buffer, std::move(lock));
        })};
        ContextLock dstBufLock{executor.tag, dstBuf};


        dstBuf.Write(src.cast<u8>(), 0, [&]() {
            executor.AttachLockedBufferView(dstBuf, std::move(dstBufLock));
            // This will prevent any CPU accesses to backing for the duration of the usage
            dstBuf.GetBuffer()->BlockAllCpuBackingWrites();

            auto srcGpuAllocation{gpu.megaBufferAllocator.Push(executor.cycle, src.cast<u8>())};
            executor.AddOutsideRpCommand([srcGpuAllocation, dstBuf, src](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &) {
                vk::BufferCopy copyRegion{
                    .size = src.size_bytes(),
                    .srcOffset = srcGpuAllocation.offset,
                    .dstOffset = dstBuf.GetOffset()
                };
                commandBuffer.copyBuffer(srcGpuAllocation.buffer, dstBuf.GetBuffer()->GetBacking(), copyRegion);
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite
                }, {}, {});
            });
        });
    }
}
