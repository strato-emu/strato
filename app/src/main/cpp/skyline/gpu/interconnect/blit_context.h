// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Ryujinx Team and Contributors (https://github.com/ryujinx/)
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/texture/format.h>
#include <gpu/texture/texture.h>
#include <gpu/texture_manager.h>
#include <gpu/buffer.h>
#include <soc/gm20b/gmmu.h>
#include <soc/gm20b/channel.h>
#include <soc/gm20b/engines/fermi/types.h>

namespace skyline::gpu::interconnect {
    using IOVA = soc::gm20b::IOVA;
    namespace fermi2d = skyline::soc::gm20b::engine::fermi2d::type;

    /**
     * @brief Handles translating Fermi 2D engine blit operations to Vulkan
     */
    class BlitContext {
      private:
        GPU &gpu;
        soc::gm20b::ChannelContext &channelCtx;
        gpu::interconnect::CommandExecutor &executor;

        gpu::GuestTexture GetGuestTexture(const fermi2d::Surface &surface) {
            auto determineFormat = [&](fermi2d::Surface::SurfaceFormat format) -> skyline::gpu::texture::Format {
                #define FORMAT_CASE(fermiFmt, skFmt, fmtType) \
                    case fermi2d::Surface::SurfaceFormat::fermiFmt ## fmtType: \
                        return skyline::gpu::format::skFmt ## fmtType

                #define FORMAT_SAME_CASE(fmt, type) FORMAT_CASE(fmt, fmt, type)

                #define FORMAT_NORM_CASE(fermiFmt, skFmt) \
                    FORMAT_CASE(fermiFmt, skFmt, Snorm); \
                    FORMAT_CASE(fermiFmt, skFmt, Unorm)

                #define FORMAT_SAME_NORM_CASE(fmt) FORMAT_NORM_CASE(fmt, fmt)

                #define FORMAT_NORM_FLOAT_CASE(fermiFmt, skFmt) \
                    FORMAT_NORM_CASE(fermiFmt, skFmt); \
                    FORMAT_CASE(fermiFmt, skFmt, Float)

                #define FORMAT_SAME_NORM_FLOAT_CASE(fmt) FORMAT_NORM_FLOAT_CASE(fmt, fmt)

                switch (format) {
                    FORMAT_SAME_NORM_CASE(R8);
                    FORMAT_SAME_NORM_FLOAT_CASE(R16);
                    FORMAT_SAME_NORM_CASE(R8G8);
                    FORMAT_SAME_CASE(B5G6R5, Unorm);
                    FORMAT_SAME_CASE(B5G5R5A1, Unorm);
                    FORMAT_SAME_CASE(R32, Float);
                    FORMAT_SAME_CASE(B10G11R11, Float);
                    FORMAT_SAME_NORM_FLOAT_CASE(R16G16);
                    FORMAT_SAME_CASE(R8G8B8A8, Unorm);
                    FORMAT_SAME_CASE(R8G8B8A8, Srgb);
                    FORMAT_NORM_CASE(R8G8B8X8, R8G8B8A8);
                    FORMAT_CASE(R8G8B8X8, R8G8B8A8, Srgb);
                    FORMAT_SAME_CASE(B8G8R8A8, Unorm);
                    FORMAT_SAME_CASE(B8G8R8A8, Srgb);
                    FORMAT_SAME_CASE(A2B10G10R10, Unorm);
                    FORMAT_SAME_CASE(R32G32, Float);
                    FORMAT_SAME_CASE(R16G16B16A16, Float);
                    FORMAT_NORM_FLOAT_CASE(R16G16B16X16, R16G16B16A16);
                    FORMAT_SAME_CASE(R32G32B32A32, Float);
                    FORMAT_CASE(R32G32B32X32, R32G32B32A32, Float);

                    default:
                        throw exception("Cannot translate the supplied surface format: 0x{:X}", static_cast<u32>(format));
                }

                #undef FORMAT_CASE
                #undef FORMAT_SAME_CASE
                #undef FORMAT_NORM_CASE
                #undef FORMAT_SAME_NORM_CASE
                #undef FORMAT_NORM_FLOAT_CASE
                #undef FORMAT_SAME_NORM_FLOAT_CASE
            };

            GuestTexture texture{};

            texture.format = determineFormat(surface.format);
            texture.aspect = texture.format->vkAspect;
            texture.baseArrayLayer = 0;
            texture.layerCount = 1;

            if (surface.memoryLayout == fermi2d::MemoryLayout::Pitch) {
                texture.type = gpu::texture::TextureType::e2D;
                texture.dimensions = gpu::texture::Dimensions{surface.stride / texture.format->bpb, surface.height, 1};
                texture.tileConfig = gpu::texture::TileConfig{
                    .mode = gpu::texture::TileMode::Pitch,
                    .pitch = surface.stride
                };
            } else {
                texture.type = gpu::texture::TextureType::e2D;
                texture.dimensions = gpu::texture::Dimensions{surface.width, surface.height, surface.depth};
                texture.tileConfig = gpu::texture::TileConfig{
                    .mode = gpu::texture::TileMode::Block,
                    .blockHeight = surface.blockSize.Height(),
                    .blockDepth = surface.blockSize.Depth(),
                };
            }

            IOVA iova{surface.address};
            size_t size{texture.GetLayerStride() * (texture.layerCount - texture.baseArrayLayer)};
            auto mappings{channelCtx.asCtx->gmmu.TranslateRange(iova, size)};
            texture.mappings.assign(mappings.begin(), mappings.end());

            return texture;
        }

      public:
        BlitContext(GPU &gpu, soc::gm20b::ChannelContext &channelCtx, gpu::interconnect::CommandExecutor &executor) : gpu(gpu), channelCtx(channelCtx), executor(executor) {}

        void Blit(const fermi2d::Surface &srcSurface, const fermi2d::Surface &dstSurface, i32 srcX, i32 srcY, i32 srcWidth, i32 srcHeight, i32 dstX, i32 dstY, i32 dstWidth, i32 dstHeight, bool resolve, bool linearFilter) {
            // TODO: OOB blit: https://github.com/Ryujinx/Ryujinx/blob/master/Ryujinx.Graphics.Gpu/Engine/Twod/TwodClass.cs#L287
            // TODO: When we support MSAA perform a resolve operation rather than blit when the `resolve` flag is set.
            auto srcGuestTexture{GetGuestTexture(srcSurface)};
            auto dstGuestTexture{GetGuestTexture(dstSurface)};

            auto srcTextureView{gpu.texture.FindOrCreate(srcGuestTexture)};
            auto dstTextureView{gpu.texture.FindOrCreate(dstGuestTexture)};

            {
                std::scoped_lock lock{*srcTextureView, *dstTextureView};

                executor.AttachTexture(&*srcTextureView);
                executor.AttachTexture(&*dstTextureView);
            }

            auto getSubresourceLayers{[](const vk::ImageSubresourceRange &range, vk::ImageAspectFlags aspect) {
                return vk::ImageSubresourceLayers{
                    .aspectMask = aspect,
                    .mipLevel = 0, // Blit engine only does one layer/mip level at a time
                    .layerCount = 1,
                    .baseArrayLayer = range.baseArrayLayer
                };
            }};

            vk::ImageBlit region{
                .srcSubresource = getSubresourceLayers(srcTextureView->range, srcTextureView->format->vkAspect),
                .dstSubresource = getSubresourceLayers(dstTextureView->range, srcTextureView->range.aspectMask),
                .srcOffsets = {{vk::Offset3D{srcX, srcY, 0}, vk::Offset3D{srcX + srcWidth, srcY + srcHeight, 1}}},
                .dstOffsets = {{vk::Offset3D{dstX, dstY, 0}, vk::Offset3D{dstX + dstWidth, dstY + dstHeight, 1}}}
            };

            executor.AddOutsideRpCommand([region, srcTextureView, dstTextureView, linearFilter](vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, GPU &) {
                std::scoped_lock lock{*srcTextureView, *dstTextureView};
                auto blitSrcImage{srcTextureView->texture->GetBacking()};
                auto blitDstImage{dstTextureView->texture->GetBacking()};

                commandBuffer.blitImage(blitSrcImage, vk::ImageLayout::eGeneral,
                                        blitDstImage, vk::ImageLayout::eGeneral,
                                        region,
                                        linearFilter ? vk::Filter::eLinear : vk::Filter::eNearest);
            });
        }
    };
}
