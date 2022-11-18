// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include "samplers.h"

namespace skyline::gpu::interconnect {
    void SamplerPoolState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, texSamplerPool, texHeaderPool);
    }

    SamplerPoolState::SamplerPoolState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void SamplerPoolState::Flush(InterconnectContext &ctx, bool useTexHeaderBinding) {
        u32 maximumIndex{useTexHeaderBinding ? engine->texHeaderPool.maximumIndex : engine->texSamplerPool.maximumIndex};
        auto mapping{ctx.channelCtx.asCtx->gmmu.LookupBlock(engine->texSamplerPool.offset)};

        texSamplers = mapping.first.subspan(mapping.second).cast<TextureSamplerControl>().first(maximumIndex + 1);

        didUseTexHeaderBinding = useTexHeaderBinding;
    }

    bool SamplerPoolState::Refresh(InterconnectContext &ctx, bool useTexHeaderBinding) {
        return didUseTexHeaderBinding != useTexHeaderBinding;
    }

    void SamplerPoolState::PurgeCaches() {
        texSamplers = span<TextureSamplerControl>{};
    }

    Samplers::Samplers(DirtyManager &manager, const SamplerPoolState::EngineRegisters &engine) : samplerPool{manager, engine} {}

    void Samplers::Update(InterconnectContext &ctx, bool useTexHeaderBinding) {
        samplerPool.Update(ctx, useTexHeaderBinding);
    }

    void Samplers::MarkAllDirty() {
        samplerPool.MarkDirty(true);
        std::fill(texSamplerCache.begin(), texSamplerCache.end(), nullptr);
    }

    static vk::Filter ConvertSamplerFilter(TextureSamplerControl::Filter filter) {
        switch (filter) {
            case TextureSamplerControl::Filter::Nearest:
                return vk::Filter::eNearest;
            case TextureSamplerControl::Filter::Linear:
                return vk::Filter::eLinear;
        }
    }

    static vk::SamplerMipmapMode ConvertSamplerMipFilter(TextureSamplerControl::MipFilter filter) {
        switch (filter) {
            // See https://github.com/yuzu-emu/yuzu/blob/5af06d14337a61d9ed1093079d13f68cbb1f5451/src/video_core/renderer_vulkan/maxwell_to_vk.cpp#L35
            case TextureSamplerControl::MipFilter::None:
                return vk::SamplerMipmapMode::eNearest;
            case TextureSamplerControl::MipFilter::Nearest:
                return vk::SamplerMipmapMode::eNearest;
            case TextureSamplerControl::MipFilter::Linear:
                return vk::SamplerMipmapMode::eLinear;
        }
    }

    static vk::SamplerAddressMode ConvertSamplerAddressMode(TextureSamplerControl::AddressMode mode) {
        switch (mode) {
            case TextureSamplerControl::AddressMode::Repeat:
                return vk::SamplerAddressMode::eRepeat;
            case TextureSamplerControl::AddressMode::MirroredRepeat:
                return vk::SamplerAddressMode::eMirroredRepeat;

            case TextureSamplerControl::AddressMode::ClampToEdge:
                return vk::SamplerAddressMode::eClampToEdge;
            case TextureSamplerControl::AddressMode::ClampToBorder:
                return vk::SamplerAddressMode::eClampToBorder;
            case TextureSamplerControl::AddressMode::Clamp:
                return vk::SamplerAddressMode::eClampToEdge; // Vulkan doesn't support 'GL_CLAMP' so this is an approximation

            case TextureSamplerControl::AddressMode::MirrorClampToEdge:
                return vk::SamplerAddressMode::eMirrorClampToEdge;
            case TextureSamplerControl::AddressMode::MirrorClampToBorder:
                return vk::SamplerAddressMode::eMirrorClampToEdge; // Only supported mirror clamps are to edges so this is an approximation
            case TextureSamplerControl::AddressMode::MirrorClamp:
                return vk::SamplerAddressMode::eMirrorClampToEdge; // Same as above
        }
    }

    static vk::CompareOp ConvertSamplerCompareOp(TextureSamplerControl::CompareOp compareOp) {
        switch (compareOp) {
            case TextureSamplerControl::CompareOp::Never:
                return vk::CompareOp::eNever;
            case TextureSamplerControl::CompareOp::Less:
                return vk::CompareOp::eLess;
            case TextureSamplerControl::CompareOp::Equal:
                return vk::CompareOp::eEqual;
            case TextureSamplerControl::CompareOp::LessOrEqual:
                return vk::CompareOp::eLessOrEqual;
            case TextureSamplerControl::CompareOp::Greater:
                return vk::CompareOp::eGreater;
            case TextureSamplerControl::CompareOp::NotEqual:
                return vk::CompareOp::eNotEqual;
            case TextureSamplerControl::CompareOp::GreaterOrEqual:
                return vk::CompareOp::eGreaterOrEqual;
            case TextureSamplerControl::CompareOp::Always:
                return vk::CompareOp::eAlways;
        }
    }

    static vk::SamplerReductionMode ConvertSamplerReductionFilter(TextureSamplerControl::SamplerReduction reduction) {
        switch (reduction) {
            case TextureSamplerControl::SamplerReduction::WeightedAverage:
                return vk::SamplerReductionMode::eWeightedAverage;
            case TextureSamplerControl::SamplerReduction::Min:
                return vk::SamplerReductionMode::eMin;
            case TextureSamplerControl::SamplerReduction::Max:
                return vk::SamplerReductionMode::eMax;
        }
    }

    static vk::BorderColor ConvertBorderColorWithCustom(float red, float green, float blue, float alpha) {
        if (alpha == 1.0f) {
            if (red == 1.0f && green == 1.0f && blue == 1.0f)
                return vk::BorderColor::eFloatOpaqueWhite;
            else if (red == 0.0f && green == 0.0f && blue == 0.0f)
                return vk::BorderColor::eFloatOpaqueBlack;
        } else if (red == 1.0f && green == 1.0f && blue == 1.0f && alpha == 0.0f) {
            return vk::BorderColor::eFloatTransparentBlack;
        }

        return vk::BorderColor::eFloatCustomEXT;
    }

    static vk::BorderColor ConvertBorderColorFixed(float red, float green, float blue, float alpha) {
        if (alpha == 1.0f) {
            if (red == 1.0f && green == 1.0f && blue == 1.0f)
                return vk::BorderColor::eFloatOpaqueWhite;
            else if (red == 0.0f && green == 0.0f && blue == 0.0f)
                return vk::BorderColor::eFloatOpaqueBlack;
        } else if (red == 1.0f && green == 1.0f && blue == 1.0f && alpha == 0.0f) {
            return vk::BorderColor::eFloatTransparentBlack;
        }

        // Approximations of a custom color using fixed colors
        if (red + green + blue > 1.0f)
            return vk::BorderColor::eFloatOpaqueWhite;
        else if (alpha > 0.0f)
            return vk::BorderColor::eFloatOpaqueBlack;
        else
            return vk::BorderColor::eFloatTransparentBlack;
    }

    vk::raii::Sampler *Samplers::GetSampler(InterconnectContext &ctx, u32 samplerIndex, u32 textureIndex) {
        const auto &samplerPoolObj{samplerPool.Get()};
        u32 index{samplerPoolObj.didUseTexHeaderBinding ? textureIndex : samplerIndex};
        auto texSamplers{samplerPoolObj.texSamplers};
        if (texSamplers.size() != texSamplerCache.size()) {
            texSamplerCache.resize(texSamplers.size());
            std::fill(texSamplerCache.begin(), texSamplerCache.end(), nullptr);
        } else if (texSamplerCache[index]) {
            return texSamplerCache[index];
        }

        TextureSamplerControl &texSampler{texSamplers[index]};
        auto &sampler{texSamplerStore[texSampler]};
        if (!sampler) {
            auto convertAddressModeWithCheck{[&](TextureSamplerControl::AddressMode mode) {
                auto vkMode{ConvertSamplerAddressMode(mode)};
                if (vkMode == vk::SamplerAddressMode::eMirrorClampToEdge && !ctx.gpu.traits.supportsSamplerMirrorClampToEdge) [[unlikely]] {
                    Logger::Warn("Cannot use Mirror Clamp To Edge as Sampler Address Mode without host GPU support");
                    return vk::SamplerAddressMode::eClampToEdge; // We use a normal clamp to edge to approximate it
                }
                return vkMode;
            }};

            auto maxAnisotropy{texSampler.MaxAnisotropy()};
            vk::StructureChain<vk::SamplerCreateInfo, vk::SamplerReductionModeCreateInfoEXT, vk::SamplerCustomBorderColorCreateInfoEXT> samplerInfo{
                vk::SamplerCreateInfo{
                    .magFilter = ConvertSamplerFilter(texSampler.magFilter),
                    .minFilter = ConvertSamplerFilter(texSampler.minFilter),
                    .mipmapMode = ConvertSamplerMipFilter(texSampler.mipFilter),
                    .addressModeU = convertAddressModeWithCheck(texSampler.addressModeU),
                    .addressModeV = convertAddressModeWithCheck(texSampler.addressModeV),
                    .addressModeW = convertAddressModeWithCheck(texSampler.addressModeP),
                    .mipLodBias = texSampler.MipLodBias(),
                    .anisotropyEnable = ctx.gpu.traits.supportsAnisotropicFiltering && maxAnisotropy > 1.0f,
                    .maxAnisotropy = maxAnisotropy,
                    .compareEnable = texSampler.depthCompareEnable,
                    .compareOp = ConvertSamplerCompareOp(texSampler.depthCompareOp),
                    .minLod = texSampler.mipFilter == TextureSamplerControl::MipFilter::None ? 0.0f : texSampler.MinLodClamp(),
                    .maxLod = texSampler.mipFilter == TextureSamplerControl::MipFilter::None ? 0.25f : texSampler.MaxLodClamp(),
                    .unnormalizedCoordinates = false,
                }, vk::SamplerReductionModeCreateInfoEXT{
                    .reductionMode = ConvertSamplerReductionFilter(texSampler.reductionFilter),
                }, vk::SamplerCustomBorderColorCreateInfoEXT{
                    .customBorderColor.float32 = {{texSampler.borderColorR, texSampler.borderColorG, texSampler.borderColorB, texSampler.borderColorA}},
                    .format = vk::Format::eUndefined,
                },
            };

            if (!ctx.gpu.traits.supportsSamplerReductionMode)
                samplerInfo.unlink<vk::SamplerReductionModeCreateInfoEXT>();

            vk::BorderColor &borderColor{samplerInfo.get<vk::SamplerCreateInfo>().borderColor};
            if (ctx.gpu.traits.supportsCustomBorderColor) {
                borderColor = ConvertBorderColorWithCustom(texSampler.borderColorR, texSampler.borderColorG, texSampler.borderColorB, texSampler.borderColorA);
                if (borderColor != vk::BorderColor::eFloatCustomEXT)
                    samplerInfo.unlink<vk::SamplerCustomBorderColorCreateInfoEXT>();
            } else {
                borderColor = ConvertBorderColorFixed(texSampler.borderColorR, texSampler.borderColorG, texSampler.borderColorB, texSampler.borderColorA);
                samplerInfo.unlink<vk::SamplerCustomBorderColorCreateInfoEXT>();
            }

            sampler = std::make_unique<vk::raii::Sampler>(ctx.gpu.vkDevice, samplerInfo.get<vk::SamplerCreateInfo>());
        }

        texSamplerCache[index] = sampler.get();
        return sampler.get();
    }

}