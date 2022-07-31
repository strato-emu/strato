// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include "command_executor.h"

namespace skyline::gpu::interconnect {
    CommandExecutor::CommandExecutor(const DeviceState &state) : gpu{*state.gpu}, activeCommandBuffer{gpu.scheduler.AllocateCommandBuffer()}, cycle{activeCommandBuffer.GetFenceCycle()}, tag{AllocateTag()} {}

    CommandExecutor::~CommandExecutor() {
        cycle->Cancel();
    }

    TextureManager &CommandExecutor::AcquireTextureManager() {
        if (!textureManagerLock)
            textureManagerLock.emplace(gpu.texture);
        return gpu.texture;
    }

    BufferManager &CommandExecutor::AcquireBufferManager() {
        if (!bufferManagerLock)
            bufferManagerLock.emplace(gpu.buffer);
        return gpu.buffer;
    }

    MegaBufferAllocator &CommandExecutor::AcquireMegaBufferAllocator() {
        if (!megaBufferAllocatorLock)
            megaBufferAllocatorLock.emplace(gpu.megaBufferAllocator);
        return gpu.megaBufferAllocator;
    }

    bool CommandExecutor::CreateRenderPassWithSubpass(vk::Rect2D renderArea, span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment) {
        auto addSubpass{[&] {
            renderPass->AddSubpass(inputAttachments, colorAttachments, depthStencilAttachment, gpu);

            lastSubpassAttachments.clear();
            auto insertAttachmentRange{[this](auto &attachments) -> std::pair<size_t, size_t> {
                size_t beginIndex{lastSubpassAttachments.size()};
                lastSubpassAttachments.insert(lastSubpassAttachments.end(), attachments.begin(), attachments.end());
                return {beginIndex, attachments.size()};
            }};

            auto rangeToSpan{[this](auto &range) -> span<TextureView *> {
                return {lastSubpassAttachments.data() + range.first, range.second};
            }};

            auto inputAttachmentRange{insertAttachmentRange(inputAttachments)};
            auto colorAttachmentRange{insertAttachmentRange(colorAttachments)};

            lastSubpassInputAttachments = rangeToSpan(inputAttachmentRange);
            lastSubpassColorAttachments = rangeToSpan(colorAttachmentRange);
            lastSubpassDepthStencilAttachment = depthStencilAttachment;
        }};

        if (renderPass == nullptr || renderPass->renderArea != renderArea || subpassCount >= gpu.traits.quirks.maxSubpassCount) {
            // We need to create a render pass if one doesn't already exist or the current one isn't compatible
            if (renderPass != nullptr)
                nodes.emplace_back(std::in_place_type_t<node::RenderPassEndNode>());
            renderPass = &std::get<node::RenderPassNode>(nodes.emplace_back(std::in_place_type_t<node::RenderPassNode>(), renderArea));
            addSubpass();
            subpassCount = 1;
            return false;
        } else {
            if (ranges::equal(lastSubpassInputAttachments, inputAttachments) &&
                ranges::equal(lastSubpassColorAttachments, colorAttachments) &&
                lastSubpassDepthStencilAttachment == depthStencilAttachment) {
                // The last subpass had the same attachments, so we can reuse them
                return false;
            } else {
                // The last subpass had different attachments, so we need to create a new one
                addSubpass();
                subpassCount++;
                return true;
            }
        }
    }

    void CommandExecutor::FinishRenderPass() {
        if (renderPass) {
            nodes.emplace_back(std::in_place_type_t<node::RenderPassEndNode>());

            renderPass = nullptr;
            subpassCount = 0;

            lastSubpassAttachments.clear();
            lastSubpassInputAttachments = nullptr;
            lastSubpassColorAttachments = nullptr;
            lastSubpassDepthStencilAttachment = nullptr;
        }
    }

    CommandExecutor::LockedTexture::LockedTexture(std::shared_ptr<Texture> texture) : texture{std::move(texture)} {}

    constexpr CommandExecutor::LockedTexture::LockedTexture(CommandExecutor::LockedTexture &&other) : texture{std::exchange(other.texture, nullptr)} {}

    constexpr Texture *CommandExecutor::LockedTexture::operator->() const {
        return texture.get();
    }

    CommandExecutor::LockedTexture::~LockedTexture() {
        if (texture)
            texture->unlock();
    }

    bool CommandExecutor::AttachTexture(TextureView *view) {
        if (!textureManagerLock)
            // Avoids a potential deadlock with this resource being locked while acquiring the TextureManager lock while the thread owning it tries to acquire a lock on this texture
            textureManagerLock.emplace(gpu.texture);

        cycle->AttachObject(view->shared_from_this());

        bool didLock{view->LockWithTag(tag)};
        if (didLock)
            attachedTextures.emplace_back(view->texture);
        return didLock;
    }

    CommandExecutor::LockedBuffer::LockedBuffer(std::shared_ptr<Buffer> buffer) : buffer{std::move(buffer)} {}

    constexpr CommandExecutor::LockedBuffer::LockedBuffer(CommandExecutor::LockedBuffer &&other) : buffer{std::exchange(other.buffer, nullptr)} {}

    constexpr Buffer *CommandExecutor::LockedBuffer::operator->() const {
        return buffer.get();
    }

    CommandExecutor::LockedBuffer::~LockedBuffer() {
        if (buffer)
            buffer->unlock();
    }

    bool CommandExecutor::AttachBuffer(BufferView &view) {
        if (!bufferManagerLock)
            // See AttachTexture(...)
            bufferManagerLock.emplace(gpu.buffer);

        bool didLock{view->LockWithTag(tag)};
        if (didLock)
            attachedBuffers.emplace_back(view->buffer);

        if (view.bufferDelegate->attached)
            return didLock;

        attachedBufferDelegates.emplace_back(view.bufferDelegate);
        view.bufferDelegate->attached = true;
        return didLock;
    }

    void CommandExecutor::AttachLockedBufferView(BufferView &view, ContextLock<BufferView> &&lock) {
        if (!bufferManagerLock)
            // See AttachTexture(...)
            bufferManagerLock.emplace(gpu.buffer);

        if (lock.OwnsLock()) {
            // Transfer ownership to executor so that the resource will stay locked for the period it is used on the GPU
            attachedBuffers.emplace_back(view->buffer);
            lock.Release(); // The executor will handle unlocking the lock so it doesn't need to be handled here
        }

        if (view.bufferDelegate->attached)
            return;

        attachedBufferDelegates.emplace_back(view.bufferDelegate);
        view.bufferDelegate->attached = true;
    }

    void CommandExecutor::AttachLockedBuffer(std::shared_ptr<Buffer> buffer, ContextLock<Buffer> &&lock) {
        if (lock.OwnsLock()) {
            attachedBuffers.emplace_back(std::move(buffer));
            lock.Release(); // See AttachLockedBufferView(...)
        }
    }

    void CommandExecutor::AttachDependency(const std::shared_ptr<void> &dependency) {
        cycle->AttachObject(dependency);
    }

    void CommandExecutor::AddSubpass(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)> &&function, vk::Rect2D renderArea, span<TextureView *> inputAttachments, span<TextureView *> colorAttachments, TextureView *depthStencilAttachment, bool exclusiveSubpass) {
        if (exclusiveSubpass)
            FinishRenderPass();

        bool gotoNext{CreateRenderPassWithSubpass(renderArea, inputAttachments, colorAttachments, depthStencilAttachment ? &*depthStencilAttachment : nullptr)};
        if (gotoNext)
            nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), std::forward<decltype(function)>(function));
        else
            nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), std::forward<decltype(function)>(function));

        if (exclusiveSubpass)
            FinishRenderPass();
    }

    void CommandExecutor::AddOutsideRpCommand(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &&function) {
        if (renderPass)
            FinishRenderPass();

        nodes.emplace_back(std::in_place_type_t<node::FunctionNode>(), std::forward<decltype(function)>(function));
    }

    void CommandExecutor::AddClearColorSubpass(TextureView *attachment, const vk::ClearColorValue &value) {
        bool gotoNext{CreateRenderPassWithSubpass(vk::Rect2D{.extent = attachment->texture->dimensions}, {}, attachment, nullptr)};
        if (renderPass->ClearColorAttachment(0, value, gpu)) {
            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[scissor = attachment->texture->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .colorAttachment = 0,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect = vk::Rect2D{.extent = scissor},
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::AddClearDepthStencilSubpass(TextureView *attachment, const vk::ClearDepthStencilValue &value) {
        bool gotoNext{CreateRenderPassWithSubpass(vk::Rect2D{.extent = attachment->texture->dimensions}, {}, {}, attachment)};
        if (renderPass->ClearDepthStencilAttachment(value, gpu)) {
            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassNode>());
        } else {
            auto function{[aspect = attachment->format->vkAspect, extent = attachment->texture->dimensions, value](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32) {
                commandBuffer.clearAttachments(vk::ClearAttachment{
                    .aspectMask = aspect,
                    .clearValue = value,
                }, vk::ClearRect{
                    .rect.extent = extent,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                });
            }};

            if (gotoNext)
                nodes.emplace_back(std::in_place_type_t<node::NextSubpassFunctionNode>(), function);
            else
                nodes.emplace_back(std::in_place_type_t<node::SubpassFunctionNode>(), function);
        }
    }

    void CommandExecutor::AddFlushCallback(std::function<void()> &&callback) {
        flushCallbacks.emplace_back(std::forward<decltype(callback)>(callback));
    }

    void CommandExecutor::SubmitInternal() {
        if (renderPass)
            FinishRenderPass();

        {
            auto &commandBuffer{*activeCommandBuffer};
            commandBuffer.begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            });

            // We need this barrier here to ensure that resources are in the state we expect them to be in, we shouldn't overwrite resources while prior commands might still be using them or read from them while they might be modified by prior commands
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, vk::MemoryBarrier{
                    .srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                }, {}, {}
            );

            for (const auto &texture : attachedTextures)
                texture->SynchronizeHostInline(commandBuffer, cycle, true);

            vk::RenderPass lRenderPass;
            u32 subpassIndex;

            using namespace node;
            for (NodeVariant &node : nodes) {
                #define NODE(name) [&](name& node) { node(commandBuffer, cycle, gpu); }
                std::visit(VariantVisitor{
                    NODE(FunctionNode),

                    [&](RenderPassNode &node) {
                        lRenderPass = node(commandBuffer, cycle, gpu);
                        subpassIndex = 0;
                    },

                    [&](NextSubpassNode &node) {
                        node(commandBuffer, cycle, gpu);
                        ++subpassIndex;
                    },
                    [&](SubpassFunctionNode &node) { node(commandBuffer, cycle, gpu, lRenderPass, subpassIndex); },
                    [&](NextSubpassFunctionNode &node) { node(commandBuffer, cycle, gpu, lRenderPass, ++subpassIndex); },

                    NODE(RenderPassEndNode),
                }, node);
                #undef NODE
            }

            commandBuffer.end();

            for (const auto &attachedBuffer : attachedBuffers)
                attachedBuffer->SynchronizeHost(); // Synchronize attached buffers from the CPU without using a staging buffer, this is done directly prior to submission to prevent stalls

            gpu.scheduler.SubmitCommandBuffer(commandBuffer, cycle);

            nodes.clear();

            for (const auto &attachedTexture : attachedTextures) {
                // We don't need to attach the Texture to the cycle as a TextureView will already be attached
                cycle->ChainCycle(attachedTexture->cycle);
                attachedTexture->cycle = cycle;
            }

            for (const auto &attachedBuffer : attachedBuffers) {
                if (attachedBuffer->RequiresCycleAttach() ) {
                    cycle->AttachObject(attachedBuffer.buffer);
                    attachedBuffer->UpdateCycle(cycle);
                }
            }
        }
    }

    void CommandExecutor::ResetInternal() {
        attachedTextures.clear();
        textureManagerLock.reset();

        for (const auto &delegate : attachedBufferDelegates) {
            delegate->usageCallbacks.reset();
            delegate->attached = false;
            delegate->view->megaBufferAllocation = {};
        }

        attachedBufferDelegates.clear();
        attachedBuffers.clear();
        bufferManagerLock.reset();
        megaBufferAllocatorLock.reset();
        allocator.Reset();
    }

    void CommandExecutor::Submit() {
        for (const auto &callback : flushCallbacks)
            callback();

        if (!nodes.empty()) {
            TRACE_EVENT("gpu", "CommandExecutor::Submit");
            SubmitInternal();
            activeCommandBuffer = gpu.scheduler.AllocateCommandBuffer();
            cycle = activeCommandBuffer.GetFenceCycle();
        }
        ResetInternal();
    }

    void CommandExecutor::SubmitWithFlush() {
        for (const auto &callback : flushCallbacks)
            callback();

        if (!nodes.empty()) {
            TRACE_EVENT("gpu", "CommandExecutor::SubmitWithFlush");
            SubmitInternal();
            cycle = activeCommandBuffer.Reset();
        }
        ResetInternal();
    }
}
