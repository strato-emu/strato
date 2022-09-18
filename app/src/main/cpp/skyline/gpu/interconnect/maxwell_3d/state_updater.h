// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/command_executor.h>
#include "common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief Header for a singly-linked state update command
     */
    struct StateUpdateCmdHeader {
        StateUpdateCmdHeader *next;
        using RecordFunc = void (*)(vk::raii::CommandBuffer &commandBuffer, StateUpdateCmdHeader *header);
        RecordFunc record;
    };

    /**
     * @brief A wrapper around a state update command that adds the required command header
     */
    template<typename Cmd>
    struct CmdHolder {
        using CmdType = Cmd;

        StateUpdateCmdHeader header{nullptr, Record};
        Cmd cmd;

        CmdHolder(Cmd &&cmd) : cmd{cmd} {}
        
        CmdHolder() = default;

        static void Record(vk::raii::CommandBuffer &commandBuffer, StateUpdateCmdHeader *header) {
            reinterpret_cast<CmdHolder *>(header)->cmd.Record(commandBuffer);
        }
    };

    struct SetVertexBuffersCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.bindVertexBuffers(firstBinding,
                                            span(buffers).subspan(firstBinding, bindingCount),
                                            span(offsets).subspan(firstBinding, bindingCount));
        }

        u32 firstBinding{};
        u32 bindingCount{};
        std::array<vk::Buffer, engine::VertexStreamCount> buffers;
        std::array<vk::DeviceSize, engine::VertexStreamCount> offsets;
    };
    using SetVertexBuffersCmd = CmdHolder<SetVertexBuffersCmdImpl>;

    struct SetVertexBuffersDynamicCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            for (u32 i{base.firstBinding}; i < base.firstBinding + base.bindingCount; i++) {
                base.buffers[i] = views[i].GetBuffer()->GetBacking();
                base.offsets[i] = views[i].GetOffset();
            }
            
            base.Record(commandBuffer);
        }

        SetVertexBuffersCmdImpl base;
        std::array<BufferView, engine::VertexStreamCount> views;
    };
    using SetVertexBuffersDynamicCmd = CmdHolder<SetVertexBuffersDynamicCmdImpl>;

    struct SetIndexBufferCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.bindIndexBuffer(buffer, offset, indexType);
        }
        
        vk::Buffer buffer;
        vk::DeviceSize offset;
        vk::IndexType indexType;
    };
    using SetIndexBufferCmd = CmdHolder<SetIndexBufferCmdImpl>;

    struct SetIndexBufferDynamicCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            base.buffer = view.GetBuffer()->GetBacking();
            base.offset = view.GetOffset();
            base.Record(commandBuffer);
        }
        
        SetIndexBufferCmdImpl base;
        BufferView view;
    };
    using SetIndexBufferDynamicCmd = CmdHolder<SetIndexBufferDynamicCmdImpl>;

    struct SetTransformFeedbackBufferCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.bindTransformFeedbackBuffersEXT(binding, buffer, offset, size);
        }
        
        u32 binding;
        vk::Buffer buffer;
        vk::DeviceSize offset;
        vk::DeviceSize size;
    };
    using SetTransformFeedbackBufferCmd = CmdHolder<SetTransformFeedbackBufferCmdImpl>;

    struct SetTransformFeedbackBufferDynamicCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            base.buffer = view.GetBuffer()->GetBacking();
            base.offset = view.GetOffset();
            base.Record(commandBuffer);
        }
        
        SetTransformFeedbackBufferCmdImpl base;
        BufferView view;
    };
    using SetTransformFeedbackBufferDynamicCmd = CmdHolder<SetTransformFeedbackBufferDynamicCmdImpl>;

    struct SetViewportCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setViewport(index, viewport);
        }
        
        u32 index;
        vk::Viewport viewport;
    };
    using SetViewportCmd = CmdHolder<SetViewportCmdImpl>;

    struct SetScissorCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setScissor(index, scissor);
        }
        
        u32 index;
        vk::Rect2D scissor;
    };
    using SetScissorCmd = CmdHolder<SetScissorCmdImpl>;
    
    struct SetLineWidthCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setLineWidth(lineWidth);
        }
        
        float lineWidth;
    };
    using SetLineWidthCmd = CmdHolder<SetLineWidthCmdImpl>;

    struct SetDepthBiasCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setDepthBias(depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
        }
        
        float depthBiasConstantFactor;
        float depthBiasClamp;
        float depthBiasSlopeFactor;
    };
    using SetDepthBiasCmd = CmdHolder<SetDepthBiasCmdImpl>;

    struct SetBlendConstantsCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setBlendConstants(blendConstants.data());
        }
        
        std::array<float, 4> blendConstants;
    };
    using SetBlendConstantsCmd = CmdHolder<SetBlendConstantsCmdImpl>;

    struct SetDepthBoundsCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setDepthBounds(minDepthBounds, maxDepthBounds);
        }
        
        float minDepthBounds;
        float maxDepthBounds;
    };
    using SetDepthBoundsCmd = CmdHolder<SetDepthBoundsCmdImpl>;

    struct SetBaseStencilStateCmdImpl {
        void Record(vk::raii::CommandBuffer &commandBuffer) {
            commandBuffer.setStencilCompareMask(flags, funcMask);
            commandBuffer.setStencilReference(flags, funcRef);
            commandBuffer.setStencilWriteMask(flags, mask);
        }
        
        vk::StencilFaceFlags flags;
        u32 funcRef;
        u32 funcMask;
        u32 mask;
    };
    using SetBaseStencilStateCmd = CmdHolder<SetBaseStencilStateCmdImpl>;

    /**
     * @brief Single-use helper for recording a batch of state updates into a command buffer
     */
    class StateUpdater {
      private:
        StateUpdateCmdHeader *first;
      
      public:
        StateUpdater(StateUpdateCmdHeader *first) : first{first} {}

        /**
         * @brief Records all contained state updates into the given command buffer
         */
        void RecordAll(vk::raii::CommandBuffer &commandBuffer) const {
            for (StateUpdateCmdHeader *cmd{first}; cmd; cmd = cmd->next)
                cmd->record(commandBuffer, cmd);
        }
    };

    /**
     * @brief Allows for quick construction of a batch of associated Vulkan state updates that can later be recorded
     */
    class StateUpdateBuilder {
      private:
        LinearAllocatorState<> &allocator;
        u32 vertexBatchBindNextBinding{};
        SetVertexBuffersDynamicCmd *vertexBatchBind{};
        StateUpdateCmdHeader *head{};
        StateUpdateCmdHeader *tail{};
        
        void AppendCmd(StateUpdateCmdHeader *cmd) {
            if (tail) {
                tail->next = cmd;
                tail = tail->next;
            } else {
                head = cmd;
                tail = head;
            }
        }
        
        template<typename Cmd>
        void AppendCmd(typename Cmd::CmdType &&contents) {
            Cmd *cmd{allocator.template EmplaceUntracked<Cmd>(std::forward<typename Cmd::CmdType>(contents))};
            AppendCmd(reinterpret_cast<StateUpdateCmdHeader *>(cmd));
        }

        void FlushVertexBatchBind() {
            if (vertexBatchBind->cmd.base.bindingCount != 0) {
                AppendCmd(reinterpret_cast<StateUpdateCmdHeader *>(vertexBatchBind));
                vertexBatchBind = allocator.EmplaceUntracked<SetVertexBuffersDynamicCmd>();
            }
        }

      public:
        StateUpdateBuilder(LinearAllocatorState<> &allocator) : allocator{allocator} {
            vertexBatchBind = allocator.EmplaceUntracked<SetVertexBuffersDynamicCmd>();
        }

        StateUpdater Build() {
            FlushVertexBatchBind();
            return {head};
        }

        void SetVertexBuffer(u32 index, const BufferBinding &binding) {
            if (index != vertexBatchBindNextBinding || vertexBatchBind->header.record != &SetVertexBuffersCmd::Record) {
                FlushVertexBatchBind();
                vertexBatchBind->header.record = &SetVertexBuffersCmd::Record;
                vertexBatchBind->cmd.base.firstBinding = index;
                vertexBatchBindNextBinding = index;
            }

            u32 bindingIdx{vertexBatchBindNextBinding++};
            vertexBatchBind->cmd.base.buffers[bindingIdx] = binding.buffer;
            vertexBatchBind->cmd.base.offsets[bindingIdx] = binding.offset;
            vertexBatchBind->cmd.base.bindingCount++;
        }

        void SetVertexBuffer(u32 index, BufferView view) {
            view.GetBuffer()->BlockSequencedCpuBackingWrites();

            if (index != vertexBatchBindNextBinding || vertexBatchBind->header.record != &SetVertexBuffersDynamicCmd::Record) {
                FlushVertexBatchBind();
                vertexBatchBind->header.record = &SetVertexBuffersDynamicCmd::Record;
                vertexBatchBind->cmd.base.firstBinding = index;
                vertexBatchBindNextBinding = index;
            }

            u32 bindingIdx{vertexBatchBindNextBinding++};
            vertexBatchBind->cmd.views[bindingIdx] = view;
            vertexBatchBind->cmd.base.bindingCount++;
        }

        void SetIndexBuffer(const BufferBinding &binding, vk::IndexType indexType) {
            AppendCmd<SetIndexBufferCmd>({
                .indexType = indexType,
                .buffer = binding.buffer,
                .offset = binding.offset,
            });
        }

        void SetIndexBuffer(BufferView view, vk::IndexType indexType) {
            view.GetBuffer()->BlockSequencedCpuBackingWrites();

            AppendCmd<SetIndexBufferDynamicCmd>({
                .base.indexType = indexType,
                .view = view,
            });
        }

        void SetTransformFeedbackBuffer(u32 index, const BufferBinding &binding) {
            AppendCmd<SetTransformFeedbackBufferCmd>({
                .binding = index,
                .buffer = binding.buffer,
                .offset = binding.offset,
            });
        }

        void SetTransformFeedbackBuffer(u32 index, BufferView view) {
            view.GetBuffer()->BlockSequencedCpuBackingWrites();

            AppendCmd<SetTransformFeedbackBufferDynamicCmd>({
                .base.binding = index,
                .view = view,
            });
        }

        void SetViewport(u32 index, const vk::Viewport &viewport) {
            AppendCmd<SetViewportCmd>({
                .index = index,
                .viewport = viewport,
            });
        }

        void SetScissor(u32 index, const vk::Rect2D &scissor) {
            AppendCmd<SetScissorCmd>({
                .index = index,
                .scissor = scissor,
            });
        }

        void SetLineWidth(float lineWidth) {
            AppendCmd<SetLineWidthCmd>({
                .lineWidth = lineWidth,
            });
        }

        void SetDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
            AppendCmd<SetDepthBiasCmd>({
                .depthBiasConstantFactor = depthBiasConstantFactor,
                .depthBiasClamp = depthBiasClamp,
                .depthBiasSlopeFactor = depthBiasSlopeFactor,
            });
        }

        void SetBlendConstants(const std::array<float, engine::BlendColorChannelCount> &blendConstants) {
            AppendCmd<SetBlendConstantsCmd>({
                .blendConstants = blendConstants,
            });
        }

        void SetDepthBounds(float minDepthBounds, float maxDepthBounds) {
            AppendCmd<SetDepthBoundsCmd>({
                .minDepthBounds = minDepthBounds,
                .maxDepthBounds = maxDepthBounds,
            });
        }

        void SetBaseStencilState(vk::StencilFaceFlags flags, u32 funcRef, u32 funcMask, u32 mask) {
            AppendCmd<SetBaseStencilStateCmd>({
                .flags = flags,
                .funcRef = funcRef,
                .funcMask = funcMask,
                .mask = mask,
            });
        }
    };
}
