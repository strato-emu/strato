// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/ryujinx/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/buffer_manager.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/channel.h>
#include "maxwell_dma.h"

namespace skyline::gpu::interconnect {
    MaxwellDma::MaxwellDma(GPU &gpu, soc::gm20b::ChannelContext &channelCtx)
        : gpu{gpu},
          channelCtx{channelCtx},
          executor{channelCtx.executor} {}

    void MaxwellDma::Copy(span<u8> dstMapping, span<u8> srcMapping) {
        auto srcBuf{gpu.buffer.FindOrCreate(srcMapping, executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
            executor.AttachLockedBuffer(buffer, std::move(lock));
        })};
        ContextLock srcBufLock{executor.tag, srcBuf};

        auto dstBuf{gpu.buffer.FindOrCreate(dstMapping, executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
            executor.AttachLockedBuffer(buffer, std::move(lock));
        })};
        ContextLock dstBufLock{executor.tag, dstBuf};

        dstBuf.CopyFrom(srcBuf, executor.usageTracker, [&]() {
            executor.AttachLockedBufferView(srcBuf, std::move(srcBufLock));
            executor.AttachLockedBufferView(dstBuf, std::move(dstBufLock));
            // This will prevent any CPU accesses to backing for the duration of the usage
            // GPU dirtiness will be handled on the CopyFrom end as it's not always necessary
            srcBuf.GetBuffer()->BlockAllCpuBackingWrites();
            dstBuf.GetBuffer()->BlockAllCpuBackingWrites();

            executor.AddOutsideRpCommand([srcBuf, dstBuf](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &gpu) {
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, vk::MemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                    .dstAccessMask = vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite
                }, {}, {});
                auto srcBufBinding{srcBuf.GetBinding(gpu)};
                auto dstBufBinding{dstBuf.GetBinding(gpu)};
                vk::BufferCopy copyRegion{
                    .size = srcBuf.size,
                    .srcOffset = srcBufBinding.offset,
                    .dstOffset = dstBufBinding.offset
                };
                commandBuffer.copyBuffer(srcBufBinding.buffer, dstBufBinding.buffer, copyRegion);
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                }, {}, {});
            });
        });
    }

    void MaxwellDma::Clear(span<u8> mapping, u32 value) {
        if (!util::IsAligned(mapping.size(), 4))
            throw exception("Cleared buffer's size is not aligned to 4 bytes!");

        auto clearBuf{gpu.buffer.FindOrCreate(mapping, executor.tag, [this](std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
            executor.AttachLockedBuffer(buffer, std::move(lock));
        })};
        executor.AttachBuffer(clearBuf);

        clearBuf.GetBuffer()->BlockSequencedCpuBackingWrites();
        clearBuf.GetBuffer()->MarkGpuDirty(executor.usageTracker);

        executor.AddOutsideRpCommand([clearBuf, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &gpu) {
            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, vk::MemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eTransferWrite
            }, {}, {});

            auto clearBufBinding{clearBuf.GetBinding(gpu)};
            commandBuffer.fillBuffer(clearBufBinding.buffer, clearBufBinding.offset, clearBufBinding.size, value);

            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            }, {}, {});
        });
    }
}
