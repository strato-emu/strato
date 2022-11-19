// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/interconnect/command_executor.h>
#include "samplers.h"
#include "textures.h"

namespace skyline::gpu::interconnect {
    union BindlessHandle {
        u32 raw;

        struct {
            u32 textureIndex : 20;
            u32 samplerIndex : 12;
        };
    };

    static DynamicBufferBinding GetConstantBufferBinding(InterconnectContext &ctx, const Shader::Info &info, BufferView view, size_t idx) {
        if (!view) // Return a dummy buffer if the constant buffer isn't bound
            return BufferBinding{ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, PAGE_SIZE).buffer, 0, PAGE_SIZE};

        ctx.executor.AttachBuffer(view);

        size_t sizeOverride{std::min<size_t>(info.constant_buffer_used_sizes[idx], view.size)};
        if (auto megaBufferBinding{view.TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionNumber, sizeOverride)}) {
            return megaBufferBinding;
        } else {
            view.GetBuffer()->BlockSequencedCpuBackingWrites();
            return view;
        }
    }

    static DynamicBufferBinding GetStorageBufferBinding(InterconnectContext &ctx, const Shader::StorageBufferDescriptor &desc, ConstantBuffer &cbuf, CachedMappedBufferView &cachedView) {
        struct SsboDescriptor {
            u64 address;
            u32 size;
        };
        static constexpr size_t MinAlignment{0x40};

        auto ssbo{cbuf.Read<SsboDescriptor>(ctx.executor, desc.cbuf_offset)};
        if (ssbo.size == 0)
            return BufferBinding{ctx.gpu.megaBufferAllocator.Allocate(ctx.executor.cycle, PAGE_SIZE).buffer, 0, PAGE_SIZE};

        size_t padding{ssbo.address & (MinAlignment - 1)};
        cachedView.Update(ctx, ssbo.address - padding, util::AlignUp(ssbo.size + padding, MinAlignment));

        auto view{cachedView.view};
        ctx.executor.AttachBuffer(view);

        if (desc.is_written) {
            view.GetBuffer()->MarkGpuDirty();
        } else {
            if (auto megaBufferBinding{view.TryMegaBuffer(ctx.executor.cycle, ctx.gpu.megaBufferAllocator, ctx.executor.executionNumber)})
                return megaBufferBinding;
        }

        view.GetBuffer()->BlockSequencedCpuBackingWrites();

        return view;
    }

    template<typename CbufSetT>
    static BindlessHandle ReadBindlessHandle(InterconnectContext &ctx, CbufSetT &constantBuffers, const auto &desc, size_t arrayIdx) {
        ConstantBuffer &primaryCbuf{constantBuffers[desc.cbuf_index]};
        size_t elemOffset{arrayIdx << desc.size_shift};
        size_t primaryCbufOffset{desc.cbuf_offset + elemOffset};
        u32 primaryVal{primaryCbuf.Read<u32>(ctx.executor, primaryCbufOffset)};

        if constexpr (requires { desc.has_secondary; }) {
            if (desc.has_secondary) {
                ConstantBuffer &secondaryCbuf{constantBuffers[desc.secondary_cbuf_index]};
                size_t secondaryCbufOffset{desc.secondary_cbuf_offset + elemOffset};
                u32 secondaryVal{secondaryCbuf.Read<u32>(ctx.executor, secondaryCbufOffset)};
                return {primaryVal | secondaryVal};
            }
        }

        return {.raw = primaryVal};
    }

    static std::pair<vk::DescriptorImageInfo, TextureView *> GetTextureBinding(InterconnectContext &ctx, const Shader::TextureDescriptor &desc, Samplers &samplers, Textures &textures, BindlessHandle handle) {
        auto sampler{samplers.GetSampler(ctx, handle.samplerIndex, handle.textureIndex)};
        auto texture{textures.GetTexture(ctx, handle.textureIndex, desc.type)};
        ctx.executor.AttachTexture(texture);
        auto view{texture->GetView()};

        return {
            vk::DescriptorImageInfo{
                .sampler = **sampler,
                .imageView = view,
                .imageLayout = texture->texture->layout
            },
            texture
        };
    }
}
