// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/descriptor_allocator.h>
#include <gpu/interconnect/common/samplers.h>
#include <gpu/interconnect/common/textures.h>
#include <soc/gm20b/gmmu.h>
#include "common.h"
#include "active_state.h"
#include "constant_buffers.h"
#include "queries.h"

namespace skyline::gpu::interconnect::maxwell3d {
    /**
     * @brief The core Maxwell 3D interconnect object, directly accessed by the engine code to perform rendering operations
     */
    class Maxwell3D {
      public:
        struct ClearEngineRegisters {
            const engine::Scissor &scissor0;
            const engine::ViewportClip &viewportClip0;
            const engine::ClearRect &clearRect;
            const std::array<u32, 4> &colorClearValue;
            const float &depthClearValue;
            const u32 &stencilClearValue;
            const engine::SurfaceClip &surfaceClip;
            const engine::ClearSurfaceControl &clearSurfaceControl;
        };

        /**
         * @brief The full set of register state used by the GPU interconnect
         */
        struct EngineRegisterBundle {
            ActiveState::EngineRegisters activeStateRegisters;
            ClearEngineRegisters clearRegisters;
            ConstantBufferSelectorState::EngineRegisters constantBufferSelectorRegisters;
            SamplerPoolState::EngineRegisters samplerPoolRegisters;
            const engine::SamplerBinding &samplerBinding;
            TexturePoolState::EngineRegisters texturePoolRegisters;
        };

      private:
        InterconnectContext ctx;
        ActiveState activeState;
        ClearEngineRegisters clearEngineRegisters;
        ConstantBuffers constantBuffers;
        Samplers samplers;
        const engine::SamplerBinding &samplerBinding;
        Textures textures;
        std::shared_ptr<memory::Buffer> quadConversionBuffer{};
        bool quadConversionBufferAttached{};
        BufferView indirectBufferView;
        Queries queries;

        static constexpr size_t DescriptorBatchSize{0x100};
        std::shared_ptr<boost::container::static_vector<DescriptorAllocator::ActiveDescriptorSet, DescriptorBatchSize>> attachedDescriptorSets;
        DescriptorAllocator::ActiveDescriptorSet *activeDescriptorSet{};
        std::vector<TextureView *> activeDescriptorSetSampledImages{};

        size_t UpdateQuadConversionBuffer(u32 count, u32 firstVertex);

        /**
         * @brief A scissor derived from the current clear register state
         */
        vk::Rect2D GetClearScissor();

        /**
         * @brief A scissor derived from the current draw register state and bound RTs
         */
        vk::Rect2D GetDrawScissor();

        /**
         * @brief Performs operations common across indirect and regular draws
         */
        void PrepareDraw(StateUpdateBuilder &builder,
                         engine::DrawTopology topology, bool indexed, bool estimateIndexBufferSize, u32 firstIndex, u32 count,
                         vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);

      public:
        DirectPipelineState &directState;

        Maxwell3D(GPU &gpu,
                  soc::gm20b::ChannelContext &channelCtx,
                  nce::NCE &nce,
                  kernel::MemoryManager &memoryManager,
                  DirtyManager &manager,
                  const EngineRegisterBundle &registerBundle);

        /**
         * @brief Loads the given data into the constant buffer pointed by the constant buffer selector starting at the given offset
         */
        void LoadConstantBuffer(span<u32> data, u32 offset);

        /**
         * @brief Binds the constant buffer selector to the given pipeline stage
         */
        void BindConstantBuffer(engine::ShaderStage stage, u32 index, bool enable);

        /**
         * @note See ConstantBuffers::DisableQuickBind
         */
        void DisableQuickConstantBufferBind();

        void Clear(engine::ClearSurface &clearSurface);

        void Draw(engine::DrawTopology topology, bool transformFeedbackEnable, bool indexed, u32 count, u32 first, u32 instanceCount, u32 vertexOffset, u32 firstInstance);

        void DrawIndirect(engine::DrawTopology topology, bool transformFeedbackEnable, bool indexed, span<u8> indirectBuffer, u32 count, u32 stride);

        void Query(soc::gm20b::IOVA address, engine::SemaphoreInfo::CounterType type, std::optional<u64> timestamp);

        void ResetCounter(engine::ClearReportValue::Type type);

        bool QueryPresentAtAddress(soc::gm20b::IOVA address);
    };
}
