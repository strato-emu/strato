// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    class ConstantBufferSelectorState : dirty::CachedManualDirty {
      public:
        struct EngineRegisters {
            const engine::ConstantBufferSelector &constantBufferSelector;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
       dirty::BoundSubresource<EngineRegisters> engine;

      public:
        ConstantBufferSelectorState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        CachedMappedBufferView view;

        void Flush(InterconnectContext &ctx);

        void PurgeCaches();
    };

    struct ConstantBuffer {
        BufferView view;

        void Read(CommandExecutor &executor, span<u8> dstBuffer, size_t srcOffset) const;

        template<typename T>
        T Read(CommandExecutor &executor, size_t srcOffset) const {
            T object;
            Read(executor, span<T>{object}.template cast<u8>(), srcOffset);
            return object;
        }
    };

    using ConstantBufferSet = std::array<std::array<ConstantBuffer, engine::ShaderStageConstantBufferCount>, engine::ShaderStageCount>;

    /**
     * @brief Holds the state of all bound constant buffers and the selector, abstracting out operations on them
     */
    class ConstantBuffers {
      private:
        dirty::ManualDirtyState<ConstantBufferSelectorState> selectorState;

      public:
        ConstantBufferSet boundConstantBuffers;

        ConstantBuffers(DirtyManager &manager, const ConstantBufferSelectorState::EngineRegisters &constantBufferSelectorRegisters);

        void MarkAllDirty();

        void Load(InterconnectContext &ctx, span<u32> data, u32 offset);

        void Bind(InterconnectContext &ctx, engine::ShaderStage stage, size_t index);

        void Unbind(engine::ShaderStage stage, size_t index);
    };
}
