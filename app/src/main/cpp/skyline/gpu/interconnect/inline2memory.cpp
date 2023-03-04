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

    void Inline2Memory::UploadSingleMapping(span<u8> dst, span<u8> src) {
        auto dstBuf{gpu.buffer.FindOrCreate(dst, executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
            executor.AttachLockedBuffer(buffer, std::move(lock));
        })};
        ContextLock dstBufLock{executor.tag, dstBuf};


        dstBuf.Write(src, 0, executor.usageTracker, [&]() {
            executor.AttachLockedBufferView(dstBuf, std::move(dstBufLock));
            // This will prevent any CPU accesses to backing for the duration of the usage
            dstBuf.GetBuffer()->BlockAllCpuBackingWrites();

            auto srcGpuAllocation{gpu.megaBufferAllocator.Push(executor.cycle, src)};
            executor.AddOutsideRpCommand([srcGpuAllocation, dstBuf, src](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &pGpu) {
                auto dstBufBinding{dstBuf.GetBinding(pGpu)};
                vk::BufferCopy copyRegion{
                    .size = src.size_bytes(),
                    .srcOffset = srcGpuAllocation.offset,
                    .dstOffset = dstBufBinding.offset,
                };
                commandBuffer.copyBuffer(srcGpuAllocation.buffer, dstBufBinding.buffer, copyRegion);
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite
                }, {}, {});
            });
        });
    }

    void Inline2Memory::Upload(IOVA dst, span<u8> src) {
        auto dstMappings{channelCtx.asCtx->gmmu.TranslateRange(dst, src.size())};

        size_t offset{};
        for (auto mapping : dstMappings) {
            UploadSingleMapping(mapping, src.subspan(offset, mapping.size()));
            offset += mapping.size();
        }
    }
}
