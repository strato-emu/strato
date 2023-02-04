// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu/buffer_manager.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include "constant_buffers.h"
#include "gpu/buffer.h"

namespace skyline::gpu::interconnect::maxwell3d {
    void ConstantBufferSelectorState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, constantBufferSelector);
    }

    ConstantBufferSelectorState::ConstantBufferSelectorState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void ConstantBufferSelectorState::Flush(InterconnectContext &ctx, size_t minSize) {
        const auto &selector{engine->constantBufferSelector};
        // Constant buffer selector size is often left at the default value of 0x10000 which can end up being larger than the underlying mapping so avoid warning for split mappings here
        view.Update(ctx, selector.address, std::max<size_t>(minSize, selector.size), false);
    }

    bool ConstantBufferSelectorState::Refresh(InterconnectContext &ctx, size_t minSize) {
        const auto &selector{engine->constantBufferSelector};
        size_t selectorMinSize{std::max<size_t>(minSize, selector.size)};
        if (view->size != selectorMinSize)
            view.Update(ctx, selector.address, selectorMinSize, false);

        return false;
    }

    void ConstantBufferSelectorState::PurgeCaches() {
        view.PurgeCaches();
    }

    ConstantBuffers::ConstantBuffers(DirtyManager &manager, const ConstantBufferSelectorState::EngineRegisters &constantBufferSelectorRegisters) : selectorState{manager, constantBufferSelectorRegisters} {}

    void ConstantBuffers::MarkAllDirty() {
        selectorState.MarkDirty(true);
    }

    void ConstantBuffers::Load(InterconnectContext &ctx, span<u32> data, u32 offset) {
        TRACE_EVENT("gpu", "ConstantBuffers::Load");

        auto &view{*selectorState.UpdateGet(ctx, data.size_bytes()).view};
        auto srcCpuBuf{data.cast<u8>()};

        ContextLock lock{ctx.executor.tag, view};

        // First attempt the write without setting up the gpu copy callback as a fast path
        if (view.Write(srcCpuBuf, offset, ctx.executor.usageTracker)) [[unlikely]] {
            // Store callback data in a stack allocated struct to avoid heap allocation for the gpu copy callback lambda
            struct GpuCopyCallbackData {
                InterconnectContext &ctx;
                span<u8> srcCpuBuf;
                u32 offset;
                ContextLock<BufferView> &lock;
                BufferView &view;
            } callbackData{ctx, srcCpuBuf, offset, lock, view};

            view.Write(srcCpuBuf, offset, ctx.executor.usageTracker, [&callbackData]() {
                callbackData.ctx.executor.AttachLockedBufferView(callbackData.view, std::move(callbackData.lock));
                // This will prevent any CPU accesses to backing for the duration of the usage
                callbackData.view.GetBuffer()->BlockAllCpuBackingWrites();

                auto srcGpuAllocation{callbackData.ctx.gpu.megaBufferAllocator.Push(callbackData.ctx.executor.cycle, callbackData.srcCpuBuf)};
                callbackData.ctx.executor.AddCheckpoint("Before constant buffer load");
                callbackData.ctx.executor.AddOutsideRpCommand([=, srcCpuBuf = callbackData.srcCpuBuf, view = callbackData.view, offset = callbackData.offset](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &gpu) {
                    auto binding{view.GetBinding(gpu)};
                    vk::BufferCopy copyRegion{
                        .size = srcCpuBuf.size_bytes(),
                        .srcOffset = srcGpuAllocation.offset,
                        .dstOffset = binding.offset + offset
                    };
                    commandBuffer.copyBuffer(srcGpuAllocation.buffer, binding.buffer, copyRegion);
                    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier{
                        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                        .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite
                    }, {}, {});
                });
                callbackData.ctx.executor.AddCheckpoint("After constant buffer load");
            });
        }
    }

    void ConstantBuffers::Bind(InterconnectContext &ctx, engine::ShaderStage stage, size_t index) {
        auto &view{*selectorState.UpdateGet(ctx).view};
        if (!view)
            throw exception("Constant buffer selector is not mapped");

        boundConstantBuffers[static_cast<size_t>(stage)][index] = {view};

        if (quickBindEnabled && quickBind)
            DisableQuickBind(); // We can only quick bind one buffer per draw
        else if (quickBindEnabled)
            quickBind = QuickBind{index, stage};
    }

    void ConstantBuffers::Unbind(engine::ShaderStage stage, size_t index) {
        boundConstantBuffers[static_cast<size_t>(stage)][index] = {};
    }

    void ConstantBuffers::ResetQuickBind() {
        quickBindEnabled = true;
        quickBind.reset();
    }

    void ConstantBuffers::DisableQuickBind() {
        quickBindEnabled = false;
        quickBind.reset();
    }
}