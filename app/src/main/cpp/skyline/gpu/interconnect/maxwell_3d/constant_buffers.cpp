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

    void ConstantBufferSelectorState::Flush(InterconnectContext &ctx) {
        const auto &selector{engine->constantBufferSelector};
        // Constant buffer selector size is often left at the default value of 0x10000 which can end up being larger than the underlying mapping so avoid warning for split mappings here
        view.Update(ctx, selector.address, selector.size, false);
    }

    void ConstantBufferSelectorState::PurgeCaches() {
        view.PurgeCaches();
    }

    ConstantBuffers::ConstantBuffers(DirtyManager &manager, const ConstantBufferSelectorState::EngineRegisters &constantBufferSelectorRegisters) : selectorState{manager, constantBufferSelectorRegisters} {}

    void ConstantBuffers::MarkAllDirty() {
        selectorState.MarkDirty(true);
    }

    static void FlushHostCallback() {
        // TODO: here we should trigger `Execute()`, however that doesn't currently work due to Read being called mid-draw and attached objects not handling this case
        Logger::Warn("GPU dirty buffer reads for attached buffers are unimplemented");
    }

    void ConstantBuffers::Load(InterconnectContext &ctx, span<u32> data, u32 offset) {
        auto &view{*selectorState.UpdateGet(ctx).view};
        auto srcCpuBuf{data.cast<u8>()};

        ContextLock lock{ctx.executor.tag, view};

        // First attempt the write without setting up the gpu copy callback as a fast path
        if (view.Write(lock.IsFirstUsage(), ctx.executor.cycle, FlushHostCallback, srcCpuBuf, offset)) [[unlikely]] {
            // Store callback data in a stack allocated struct to avoid heap allocation for the gpu copy callback lambda
            struct GpuCopyCallbackData {
                InterconnectContext &ctx;
                span<u8> srcCpuBuf;
                u32 offset;
                ContextLock<BufferView> &lock;
                BufferView &view;
            } callbackData{ctx, srcCpuBuf, offset, lock, view};

            view.Write(lock.IsFirstUsage(), ctx.executor.cycle, FlushHostCallback, srcCpuBuf, offset, [&callbackData]() {
                callbackData.ctx.executor.AttachLockedBufferView(callbackData.view, std::move(callbackData.lock));
                // This will prevent any CPU accesses to backing for the duration of the usage
                callbackData.view.GetBuffer()->BlockAllCpuBackingWrites();

                auto srcGpuAllocation{callbackData.ctx.executor.AcquireMegaBufferAllocator().Push(callbackData.ctx.executor.cycle, callbackData.srcCpuBuf)};
                callbackData.ctx.executor.AddOutsideRpCommand([=, srcCpuBuf = callbackData.srcCpuBuf, view = callbackData.view, offset = callbackData.offset](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &) {
                    vk::BufferCopy copyRegion{
                        .size = srcCpuBuf.size_bytes(),
                        .srcOffset = srcGpuAllocation.offset,
                        .dstOffset = view.GetOffset() + offset
                    };
                    commandBuffer.copyBuffer(srcGpuAllocation.buffer, view.GetBuffer()->GetBacking(), copyRegion);
                });
            });
        }
    }

    void ConstantBuffers::Bind(InterconnectContext &ctx, engine::ShaderStage stage, size_t index) {
        auto &view{*selectorState.UpdateGet(ctx).view};
        boundConstantBuffers[static_cast<size_t>(stage)][index] = {view};
    }

    void ConstantBuffers::Unbind(engine::ShaderStage stage, size_t index) {
        boundConstantBuffers[static_cast<size_t>(stage)][index] = {};
    }
}