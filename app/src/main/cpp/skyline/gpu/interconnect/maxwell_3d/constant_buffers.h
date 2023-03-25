// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "common.h"

namespace skyline::gpu::interconnect::maxwell3d {
    class ConstantBufferSelectorState : dirty::CachedManualDirty, dirty::RefreshableManualDirty {
      public:
        struct EngineRegisters {
            const engine::ConstantBufferSelector &constantBufferSelector;

            void DirtyBind(DirtyManager &manager, dirty::Handle handle) const;
        };

      private:
        dirty::BoundSubresource <EngineRegisters> engine;

      public:
        ConstantBufferSelectorState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine);

        CachedMappedBufferView view{};

        void Flush(InterconnectContext &ctx, size_t minSize = 0);

        bool Refresh(InterconnectContext &ctx, size_t minSize = 0);

        void PurgeCaches();
    };

    using ConstantBufferSet = std::array<std::array<ConstantBuffer, engine::ShaderStageConstantBufferCount>, engine::ShaderStageCount>;

    /**
     * @brief Holds the state of all bound constant buffers and the selector, abstracting out operations on them
     */
    class ConstantBuffers {
      private:
        dirty::ManualDirtyState <ConstantBufferSelectorState> selectorState;

      public:
        ConstantBufferSet boundConstantBuffers;

        /**
         * @brief Allows for a single constant buffer to be bound between two draws without requiring a full descriptor sync
         */
        struct QuickBind {
            size_t index; //!< The index of the constant buffer to bind
            engine::ShaderStage stage; //!< The shader stage to bind the constant buffer to
        };
        std::optional<QuickBind> quickBind;
        bool quickBindEnabled{}; //!< If quick binding can occur, if multiple bindings, constant buffer loads or other engines have been used since the last draw this is disabled

        ConstantBuffers(DirtyManager &manager, const ConstantBufferSelectorState::EngineRegisters &constantBufferSelectorRegisters);

        void MarkAllDirty();

        void Load(InterconnectContext &ctx, span <u32> data, u32 offset);

        void Bind(InterconnectContext &ctx, engine::ShaderStage stage, size_t index);

        void Unbind(engine::ShaderStage stage, size_t index);

        /**
         * @brief Resets quick binding state to be ready store a new bind, this should be called after every draw
         */
        void ResetQuickBind();

        /**
         * @brief Diables quick binding, this should be called before any operation that could impact contents of bound constant buffers
         */
        void DisableQuickBind();
    };
}
