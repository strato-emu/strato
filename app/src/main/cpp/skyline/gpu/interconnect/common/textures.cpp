// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc/gm20b/channel.h>
#include <soc/gm20b/gmmu.h>
#include <gpu/texture_manager.h>
#include <gpu/texture/format.h>
#include "textures.h"

namespace skyline::gpu::interconnect {
    void TexturePoolState::EngineRegisters::DirtyBind(DirtyManager &manager, dirty::Handle handle) const {
        manager.Bind(handle, texHeaderPool);
    }

    TexturePoolState::TexturePoolState(dirty::Handle dirtyHandle, DirtyManager &manager, const EngineRegisters &engine) : engine{manager, dirtyHandle, engine} {}

    void TexturePoolState::Flush(InterconnectContext &ctx) {
        auto mapping{ctx.channelCtx.asCtx->gmmu.LookupBlock(engine->texHeaderPool.offset)};

        textureHeaders = mapping.first.subspan(mapping.second).cast<TextureImageControl>().first(engine->texHeaderPool.maximumIndex + 1);
    }

    void TexturePoolState::PurgeCaches() {
        textureHeaders = span<TextureImageControl>{};
    }

    Textures::Textures(DirtyManager &manager, const TexturePoolState::EngineRegisters &engine) : texturePool{manager, engine} {}

    void Textures::MarkAllDirty() {
        texturePool.MarkDirty(true);
    }

    static texture::Format ConvertTicFormat(TextureImageControl::FormatWord format, bool srgb) {
        using TIC = TextureImageControl;
        #define TIC_FORMAT(fmt, compR, compG, compB, compA, srgb) \
                TIC::FormatWord{ .format = TIC::ImageFormat::fmt,          \
                                 .componentR = TIC::ImageComponent::compR, \
                                 .componentG = TIC::ImageComponent::compG, \
                                 .componentB = TIC::ImageComponent::compB, \
                                 .componentA = TIC::ImageComponent::compA, \
                                 ._pad_ = srgb }.Raw() // Reuse _pad_ to store if the texture is sRGB

        // For formats where all components are of the same type
        #define TIC_FORMAT_ST(format, component) \
                TIC_FORMAT(format, component, component, component, component, false)

        #define TIC_FORMAT_ST_SRGB(format, component) \
                TIC_FORMAT(format, component, component, component, component, true)

        #define TIC_FORMAT_CASE(ticFormat, skFormat, componentR, componentG, componentB, componentA)  \
                case TIC_FORMAT(ticFormat, componentR, componentG, componentB, componentA, false): \
                    return format::skFormat

        #define TIC_FORMAT_CASE_ST(ticFormat, skFormat, component)  \
                case TIC_FORMAT_ST(ticFormat, component): \
                    return format::skFormat ## component

        #define TIC_FORMAT_CASE_ST_SRGB(ticFormat, skFormat, component)  \
                case TIC_FORMAT_ST_SRGB(ticFormat, component): \
                    return format::skFormat ## Srgb

        #define TIC_FORMAT_CASE_NORM(ticFormat, skFormat)  \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Unorm); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Snorm)

        #define TIC_FORMAT_CASE_INT(ticFormat, skFormat)  \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Uint); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Sint)

        #define TIC_FORMAT_CASE_NORM_INT(ticFormat, skFormat) \
                TIC_FORMAT_CASE_NORM(ticFormat, skFormat); \
                TIC_FORMAT_CASE_INT(ticFormat, skFormat)

        #define TIC_FORMAT_CASE_NORM_INT_FLOAT(ticFormat, skFormat) \
                TIC_FORMAT_CASE_NORM_INT(ticFormat, skFormat); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Float)

        #define TIC_FORMAT_CASE_INT_FLOAT(ticFormat, skFormat) \
                TIC_FORMAT_CASE_INT(ticFormat, skFormat); \
                TIC_FORMAT_CASE_ST(ticFormat, skFormat, Float)

        // Ignore the swizzle components of the format word
        format._pad_ = srgb; // Reuse the _pad_ field to store the srgb flag
        switch ((format.Raw() & TextureImageControl::FormatWord::FormatColorComponentPadMask)) {
            TIC_FORMAT_CASE_NORM_INT(R8, R8);

            TIC_FORMAT_CASE_NORM_INT_FLOAT(R16, R16);
            TIC_FORMAT_CASE_ST(D16, D16, Unorm);
            TIC_FORMAT_CASE_NORM_INT(R8G8, R8G8);
            TIC_FORMAT_CASE_ST(B5G6R5, B5G6R5, Unorm);
            TIC_FORMAT_CASE_ST(R4G4B4A4, R4G4B4A4, Unorm);
            TIC_FORMAT_CASE_ST(A1B5G5R5, A1B5G5R5, Unorm);

            TIC_FORMAT_CASE_INT_FLOAT(R32, R32);
            TIC_FORMAT_CASE_ST(D32, D32, Float);
            TIC_FORMAT_CASE_NORM_INT_FLOAT(R16G16, R16G16);
            TIC_FORMAT_CASE(R8G24, S8UintD24Unorm, Uint, Unorm, Unorm, Unorm);
            TIC_FORMAT_CASE(S8D24, S8UintD24Unorm, Uint, Unorm, Uint, Uint);
            TIC_FORMAT_CASE(S8D24, S8UintD24Unorm, Uint, Unorm, Unorm, Unorm);
            TIC_FORMAT_CASE(D24S8, D24UnormS8Uint, Unorm, Uint, Uint, Uint);

            TIC_FORMAT_CASE_ST(B10G11R11, B10G11R11, Float);
            TIC_FORMAT_CASE_NORM_INT(A8B8G8R8, R8G8B8A8);
            TIC_FORMAT_CASE_ST_SRGB(A8B8G8R8, R8G8B8A8, Unorm);
            TIC_FORMAT_CASE_NORM_INT(A2B10G10R10, A2B10G10R10);
            TIC_FORMAT_CASE_ST(E5B9G9R9, E5B9G9R9, Float);

            TIC_FORMAT_CASE_ST(BC1, BC1, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(BC1, BC1, Unorm);
            TIC_FORMAT_CASE_NORM(BC4, BC4);
            TIC_FORMAT_CASE_INT_FLOAT(R32G32, R32G32);
            TIC_FORMAT_CASE(D32S8, D32FloatS8Uint, Float, Uint, Uint, Unorm);
            TIC_FORMAT_CASE(D32S8, D32FloatS8Uint, Float, Uint, Unorm, Unorm);
            TIC_FORMAT_CASE(R32B24G8, D32FloatS8Uint, Float, Uint, Unorm, Unorm);

            TIC_FORMAT_CASE_NORM_INT_FLOAT(R16G16B16A16, R16G16B16A16);

            TIC_FORMAT_CASE_ST(Astc4x4, Astc4x4, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc4x4, Astc4x4, Unorm);
            TIC_FORMAT_CASE_ST(Astc5x4, Astc5x4, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc5x4, Astc5x4, Unorm);
            TIC_FORMAT_CASE_ST(Astc5x5, Astc5x5, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc5x5, Astc5x5, Unorm);
            TIC_FORMAT_CASE_ST(Astc6x5, Astc6x5, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc6x5, Astc6x5, Unorm);
            TIC_FORMAT_CASE_ST(Astc6x6, Astc6x6, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc6x6, Astc6x6, Unorm);
            TIC_FORMAT_CASE_ST(Astc8x5, Astc8x5, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc8x5, Astc8x5, Unorm);
            TIC_FORMAT_CASE_ST(Astc8x6, Astc8x6, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc8x6, Astc8x6, Unorm);
            TIC_FORMAT_CASE_ST(Astc8x8, Astc8x8, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc8x8, Astc8x8, Unorm);
            TIC_FORMAT_CASE_ST(Astc10x5, Astc10x5, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc10x5, Astc10x5, Unorm);
            TIC_FORMAT_CASE_ST(Astc10x6, Astc10x6, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc10x6, Astc10x6, Unorm);
            TIC_FORMAT_CASE_ST(Astc10x8, Astc10x8, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc10x8, Astc10x8, Unorm);
            TIC_FORMAT_CASE_ST(Astc10x10, Astc10x10, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc10x10, Astc10x10, Unorm);
            TIC_FORMAT_CASE_ST(Astc12x10, Astc12x10, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(Astc12x10, Astc12x10, Unorm);
            TIC_FORMAT_CASE_ST(Astc12x12, Astc12x12, Unorm);
	        TIC_FORMAT_CASE_ST_SRGB(Astc12x12, Astc12x12, Unorm);

            TIC_FORMAT_CASE_ST(BC2, BC2, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(BC2, BC2, Unorm);
            TIC_FORMAT_CASE_ST(BC3, BC3, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(BC3, BC3, Unorm);
            TIC_FORMAT_CASE_NORM(BC5, BC5);
            TIC_FORMAT_CASE(Bc6HUfloat, Bc6HUfloat, Float, Float, Float, Float);
            TIC_FORMAT_CASE(Bc6HSfloat, Bc6HSfloat, Float, Float, Float, Float);
            TIC_FORMAT_CASE_ST(BC7, BC7, Unorm);
            TIC_FORMAT_CASE_ST_SRGB(BC7, BC7, Unorm);

            TIC_FORMAT_CASE_INT_FLOAT(R32G32B32A32, R32G32B32A32);

            default:
                if (format.Raw())
                    Logger::Error("Cannot translate TIC format: 0x{:X}", static_cast<u32>(format.Raw()));
                return {};
        }

        #undef TIC_FORMAT
        #undef TIC_FORMAT_ST
        #undef TIC_FORMAT_CASE
        #undef TIC_FORMAT_CASE_ST
        #undef TIC_FORMAT_CASE_NORM
        #undef TIC_FORMAT_CASE_INT
        #undef TIC_FORMAT_CASE_NORM_INT
        #undef TIC_FORMAT_CASE_NORM_INT_FLOAT
    }

    static vk::ComponentMapping ConvertTicSwizzleMapping(TextureImageControl::FormatWord format, vk::ComponentMapping swizzleMapping) {
        auto convertComponentSwizzle{[swizzleMapping](TextureImageControl::ImageSwizzle swizzle) {
            switch (swizzle) {
                case TextureImageControl::ImageSwizzle::R:
                    return swizzleMapping.r;
                case TextureImageControl::ImageSwizzle::G:
                    return swizzleMapping.g;
                case TextureImageControl::ImageSwizzle::B:
                    return swizzleMapping.b;
                case TextureImageControl::ImageSwizzle::A:
                    return swizzleMapping.a;
                case TextureImageControl::ImageSwizzle::Zero:
                    return vk::ComponentSwizzle::eZero;
                case TextureImageControl::ImageSwizzle::OneFloat:
                case TextureImageControl::ImageSwizzle::OneInt:
                    return vk::ComponentSwizzle::eOne;
                default:
                    throw exception("Invalid swizzle: {:X}", static_cast<u32>(swizzle));
            }
        }};

        return vk::ComponentMapping{
            .r = convertComponentSwizzle(format.swizzleX),
            .g = convertComponentSwizzle(format.swizzleY),
            .b = convertComponentSwizzle(format.swizzleZ),
            .a = convertComponentSwizzle(format.swizzleW)
        };
    }

    static std::shared_ptr<TextureView> CreateNullTexture(InterconnectContext &ctx) {
        constexpr texture::Format NullImageFormat{format::R8G8B8A8Unorm};
        constexpr texture::Dimensions NullImageDimensions{1, 1, 1};
        constexpr vk::ImageLayout NullImageInitialLayout{vk::ImageLayout::eUndefined};
        constexpr vk::ImageTiling NullImageTiling{vk::ImageTiling::eOptimal};
        constexpr vk::ImageCreateFlags NullImageFlags{};
        constexpr vk::ImageUsageFlags NullImageUsage{vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled};

        auto vkImage{ctx.gpu.memory.AllocateImage(
            {
                .flags = NullImageFlags,
                .imageType = vk::ImageType::e2D,
                .format = NullImageFormat->vkFormat,
                .extent = NullImageDimensions,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = vk::SampleCountFlagBits::e1,
                .tiling = NullImageTiling,
                .usage = NullImageUsage,
                .sharingMode = vk::SharingMode::eExclusive,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &ctx.gpu.vkQueueFamilyIndex,
                .initialLayout = NullImageInitialLayout
            }
        )};

        auto nullTexture{std::make_shared<Texture>(ctx.gpu, std::move(vkImage), NullImageDimensions, NullImageFormat, NullImageInitialLayout, NullImageTiling, NullImageFlags, NullImageUsage)};
        nullTexture->TransitionLayout(vk::ImageLayout::eGeneral);
        return nullTexture->GetView(vk::ImageViewType::e2D, vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1,
        });;
    }

    TextureView *Textures::GetTexture(InterconnectContext &ctx, u32 index, Shader::TextureType shaderType) {
        auto textureHeaders{texturePool.UpdateGet(ctx).textureHeaders};
        if (textureHeaderCache.size() != textureHeaders.size()) {
            textureHeaderCache.resize(textureHeaders.size());
            std::fill(textureHeaderCache.begin(), textureHeaderCache.end(), CacheEntry{});
        } else if (textureHeaders.size() > index && textureHeaderCache[index].view) {
            auto &cached{textureHeaderCache[index]};
            if (cached.sequenceNumber == ctx.channelCtx.channelSequenceNumber)
                return cached.view;

            if (cached.tic == textureHeaders[index] && !cached.view->texture->replaced) {
                cached.sequenceNumber = ctx.channelCtx.channelSequenceNumber;
                return cached.view;
            }
        }

        if (index >= textureHeaders.size()) {
            if (!nullTextureView)
                nullTextureView = CreateNullTexture(ctx);

            return nullTextureView.get();
        }

        TextureImageControl &textureHeader{textureHeaders[index]};
        auto &texture{textureHeaderStore[textureHeader]};

        if (!texture || texture->texture->replaced) {
            // If the entry didn't exist prior then we need to convert the TIC to a GuestTexture
            GuestTexture guest{};
            if (auto format{ConvertTicFormat(textureHeader.formatWord, textureHeader.isSrgb)}) {
                guest.format = format;
            } else {
                if (!nullTextureView)
                    nullTextureView = CreateNullTexture(ctx);

                return nullTextureView.get();
            }

            guest.aspect = guest.format->Aspect(textureHeader.formatWord.swizzleX == TextureImageControl::ImageSwizzle::R);
            guest.swizzle = ConvertTicSwizzleMapping(textureHeader.formatWord, guest.format->swizzleMapping);

            constexpr size_t CubeFaceCount{6};

            guest.baseArrayLayer = static_cast<u16>(textureHeader.BaseLayer());
            guest.dimensions = texture::Dimensions(textureHeader.widthMinusOne + 1, textureHeader.heightMinusOne + 1, 1);
            u16 depth{static_cast<u16>(textureHeader.depthMinusOne + 1)};

            guest.mipLevelCount = textureHeader.mipMaxLevels + 1;
            guest.viewMipBase = textureHeader.viewConfig.mipMinLevel;
            guest.viewMipCount = textureHeader.viewConfig.mipMaxLevel - textureHeader.viewConfig.mipMinLevel + 1;

            switch (textureHeader.textureType) {
                case TextureImageControl::TextureType::e1D:
                    guest.viewType = shaderType == Shader::TextureType::ColorArray1D ? vk::ImageViewType::e1DArray : vk::ImageViewType::e1D;
                    guest.layerCount = 1;
                    break;
                case TextureImageControl::TextureType::e1DArray:
                    guest.viewType = vk::ImageViewType::e1DArray;
                    guest.layerCount = depth;
                    break;
                case TextureImageControl::TextureType::e1DBuffer:
                    throw exception("1D Buffers are not supported");

                case TextureImageControl::TextureType::e2DNoMipmap:
                    guest.mipLevelCount = 1;
                    guest.viewMipBase = 0;
                    guest.viewMipCount = 1;
                case TextureImageControl::TextureType::e2D:
                    guest.viewType = shaderType == Shader::TextureType::ColorArray2D ? vk::ImageViewType::e2DArray : vk::ImageViewType::e2D;
                    guest.layerCount = 1;
                    break;
                case TextureImageControl::TextureType::e2DArray:
                    guest.viewType = vk::ImageViewType::e2DArray;
                    guest.layerCount = depth;
                    break;

                case TextureImageControl::TextureType::e3D:
                    guest.viewType = vk::ImageViewType::e3D;
                    guest.layerCount = 1;
                    guest.dimensions.depth = depth;
                    break;

                case TextureImageControl::TextureType::eCube:
                    guest.viewType = shaderType == Shader::TextureType::ColorArrayCube ? vk::ImageViewType::eCubeArray : vk::ImageViewType::eCube;
                    guest.layerCount = CubeFaceCount;
                    break;
                case TextureImageControl::TextureType::eCubeArray:
                    guest.viewType = vk::ImageViewType::eCubeArray;
                    guest.layerCount = depth * CubeFaceCount;
                    break;
            }

            size_t size; //!< The size of the texture in bytes
            if (textureHeader.headerType == TextureImageControl::HeaderType::Pitch) {
                guest.tileConfig = {
                    .mode = texture::TileMode::Pitch,
                    .pitch = static_cast<u32>(textureHeader.tileConfig.pitchHigh) << TextureImageControl::TileConfig::PitchAlignmentBits,
                };
            } else if (textureHeader.headerType == TextureImageControl::HeaderType::BlockLinear) {
                guest.tileConfig = {
                    .mode = texture::TileMode::Block,
                    .blockHeight = static_cast<u8>(1U << textureHeader.tileConfig.tileHeightGobsLog2),
                    .blockDepth = static_cast<u8>(1U << textureHeader.tileConfig.tileDepthGobsLog2),
                };
            } else {
                throw exception("Unsupported TIC Header Type: {}", static_cast<u32>(textureHeader.headerType));
            }

            auto mappings{ctx.channelCtx.asCtx->gmmu.TranslateRange(textureHeader.Iova(), guest.GetSize())};
            guest.mappings.assign(mappings.begin(), mappings.end());
            if (guest.mappings.empty() || !std::all_of(guest.mappings.begin(), guest.mappings.end(), [](auto map) { return map.valid(); }) || guest.mappings.front().empty()) {
                Logger::Warn("Unmapped texture in pool: 0x{:X}", textureHeader.Iova());
                if (!nullTextureView)
                    nullTextureView = CreateNullTexture(ctx);

                return nullTextureView.get();
            }
            texture = ctx.gpu.texture.FindOrCreate(guest, ctx.executor.tag);
        }

        textureHeaderCache[index] = {textureHeader, texture.get(), ctx.channelCtx.channelSequenceNumber};
        return texture.get();
    }

    Shader::TextureType Textures::GetTextureType(InterconnectContext &ctx, u32 index) {
        auto textureHeaders{texturePool.UpdateGet(ctx).textureHeaders};
        switch (textureHeaders[index].textureType) {
            case TextureImageControl::TextureType::e1D:
                return Shader::TextureType::Color1D;
            case TextureImageControl::TextureType::e1DArray:
                return Shader::TextureType::ColorArray1D;
            case TextureImageControl::TextureType::e1DBuffer:
                return Shader::TextureType::Buffer;
            case TextureImageControl::TextureType::e2DNoMipmap:
            case TextureImageControl::TextureType::e2D:
                return Shader::TextureType::Color2D;
            case TextureImageControl::TextureType::e2DArray:
                return Shader::TextureType::ColorArray2D;
            case TextureImageControl::TextureType::e3D:
                return Shader::TextureType::Color3D;
            case TextureImageControl::TextureType::eCube:
                return Shader::TextureType::ColorCube;
            case TextureImageControl::TextureType::eCubeArray:
                return Shader::TextureType::ColorArrayCube;
        }
    }
}
