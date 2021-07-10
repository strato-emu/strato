// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <common/trace.h>
#include <kernel/types/KProcess.h>
#include "texture.h"

namespace skyline::gpu {
    GuestTexture::GuestTexture(const DeviceState &state, u8 *pointer, texture::Dimensions dimensions, const texture::Format &format, texture::TileMode tiling, texture::TileConfig layout) : state(state), pointer(pointer), dimensions(dimensions), format(format), tileMode(tiling), tileConfig(layout) {}

    std::shared_ptr<Texture> GuestTexture::InitializeTexture(vk::Image backing, texture::Dimensions pDimensions, const texture::Format &pFormat, std::optional<vk::ImageTiling> tiling, vk::ImageLayout layout, texture::Swizzle swizzle) {
        if (!host.expired())
            throw exception("Trying to create multiple Texture objects from a single GuestTexture");
        auto sharedHost{std::make_shared<Texture>(*state.gpu, backing, shared_from_this(), pDimensions ? pDimensions : dimensions, pFormat ? pFormat : format, layout, tiling ? *tiling : (tileMode == texture::TileMode::Block) ? vk::ImageTiling::eOptimal : vk::ImageTiling::eLinear, swizzle)};
        host = sharedHost;
        return sharedHost;
    }

    std::shared_ptr<Texture> GuestTexture::InitializeTexture(vk::raii::Image &&backing, std::optional<vk::ImageTiling> tiling, vk::ImageLayout layout, const texture::Format &pFormat, texture::Dimensions pDimensions, texture::Swizzle swizzle) {
        if (!host.expired())
            throw exception("Trying to create multiple Texture objects from a single GuestTexture");
        auto sharedHost{std::make_shared<Texture>(*state.gpu, std::move(backing), shared_from_this(), pDimensions ? pDimensions : dimensions, pFormat ? pFormat : format, layout, tiling ? *tiling : (tileMode == texture::TileMode::Block) ? vk::ImageTiling::eOptimal : vk::ImageTiling::eLinear, swizzle)};
        host = sharedHost;
        return sharedHost;
    }

    std::shared_ptr<Texture> GuestTexture::CreateTexture(vk::ImageUsageFlags usage, std::optional<vk::ImageTiling> pTiling, vk::ImageLayout initialLayout, const texture::Format &pFormat, texture::Dimensions pDimensions, texture::Swizzle swizzle) {
        if (!host.expired())
            throw exception("Trying to create multiple Texture objects from a single GuestTexture");

        pDimensions = pDimensions ? pDimensions : dimensions;
        const auto &lFormat{pFormat ? pFormat : format};
        auto tiling{pTiling ? *pTiling : (tileMode == texture::TileMode::Block) ? vk::ImageTiling::eOptimal : vk::ImageTiling::eLinear};
        vk::ImageCreateInfo imageCreateInfo{
            .imageType = pDimensions.GetType(),
            .format = lFormat,
            .extent = pDimensions,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = tiling,
            .usage = usage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &state.gpu->vkQueueFamilyIndex,
            .initialLayout = initialLayout,
        };

        auto sharedHost{std::make_shared<Texture>(*state.gpu, tiling != vk::ImageTiling::eLinear ? state.gpu->memory.AllocateImage(imageCreateInfo) : state.gpu->memory.AllocateMappedImage(imageCreateInfo), shared_from_this(), pDimensions, lFormat, initialLayout, tiling, swizzle)};
        host = sharedHost;
        return sharedHost;
    }

    Texture::Texture(GPU &gpu, BackingType &&backing, std::shared_ptr<GuestTexture> guest, texture::Dimensions dimensions, const texture::Format &format, vk::ImageLayout layout, vk::ImageTiling tiling, vk::ComponentMapping mapping) : gpu(gpu), backing(std::move(backing)), layout(layout), guest(std::move(guest)), dimensions(dimensions), format(format), tiling(tiling), mapping(mapping) {
        if (GetBacking())
            SynchronizeHost();
    }

    Texture::Texture(GPU &gpu, BackingType &&backing, texture::Dimensions dimensions, const texture::Format &format, vk::ImageLayout layout, vk::ImageTiling tiling, vk::ComponentMapping mapping) : gpu(gpu), backing(std::move(backing)), guest(nullptr), dimensions(dimensions), format(format), layout(layout), tiling(tiling), mapping(mapping) {}

    Texture::Texture(GPU &gpu, texture::Dimensions dimensions, const texture::Format &format, vk::ImageLayout initialLayout, vk::ImageUsageFlags usage, vk::ImageTiling tiling, vk::ComponentMapping mapping) : gpu(gpu), guest(nullptr), dimensions(dimensions), format(format), layout(initialLayout), tiling(tiling), mapping(mapping) {
        vk::ImageCreateInfo imageCreateInfo{
            .imageType = dimensions.GetType(),
            .format = format,
            .extent = dimensions,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = tiling,
            .usage = usage | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &gpu.vkQueueFamilyIndex,
            .initialLayout = initialLayout,
        };
        backing = tiling != vk::ImageTiling::eLinear ? gpu.memory.AllocateImage(imageCreateInfo) : gpu.memory.AllocateMappedImage(imageCreateInfo);
    }

    bool Texture::WaitOnBacking() {
        if (GetBacking()) [[likely]] {
            return false;
        } else {
            std::unique_lock lock(mutex, std::adopt_lock);
            backingCondition.wait(lock, [&]() -> bool { return GetBacking(); });
            lock.release();
            return true;
        }
    }

    void Texture::WaitOnFence() {
        if (cycle) {
            cycle->Wait();
            cycle.reset();
        }
    }

    void Texture::SwapBacking(BackingType &&pBacking, vk::ImageLayout pLayout) {
        WaitOnFence();

        backing = std::move(pBacking);
        layout = pLayout;
        if (GetBacking())
            backingCondition.notify_all();
    }

    void Texture::TransitionLayout(vk::ImageLayout pLayout) {
        WaitOnBacking();
        WaitOnFence();

        if (layout != pLayout) {
            cycle = gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                commandBuffer.pipelineBarrier(layout != vk::ImageLayout::eUndefined ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, vk::ImageMemoryBarrier{
                    .image = GetBacking(),
                    .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                    .oldLayout = layout,
                    .newLayout = pLayout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });
            });
            layout = pLayout;
        }
    }

    void Texture::SynchronizeHost() {
        if (!guest)
            throw exception("Synchronization of host textures requires a valid guest texture to synchronize from");

        TRACE_EVENT("gpu", "Texture::SynchronizeHost");
        auto pointer{guest->pointer};
        auto size{format.GetSize(dimensions)};

        u8 *bufferData;
        auto stagingBuffer{[&]() -> std::shared_ptr<memory::StagingBuffer> {
            if (tiling == vk::ImageTiling::eOptimal || !std::holds_alternative<memory::Image>(backing)) {
                // We need a staging buffer for all optimal copies (since we aren't aware of the host optimal layout) and linear textures which we cannot map on the CPU since we do not have access to their backing VkDeviceMemory
                auto stagingBuffer{gpu.memory.AllocateStagingBuffer(size)};
                bufferData = stagingBuffer->data();
                return stagingBuffer;
            } else if (tiling == vk::ImageTiling::eLinear) {
                // We can optimize linear texture sync on a UMA by mapping the texture onto the CPU and copying directly into it rather than a staging buffer
                bufferData = std::get<memory::Image>(backing).data();
                WaitOnFence(); // We need to wait on fence here since we are mutating the texture directly after, the wait can be deferred till the copy when a staging buffer is used
                return nullptr;
            } else {
                throw exception("Guest -> Host synchronization of images tiled as '{}' isn't implemented", vk::to_string(tiling));
            }
        }()};

        if (guest->tileMode == texture::TileMode::Block) {
            // Reference on Block-linear tiling: https://gist.github.com/PixelyIon/d9c35050af0ef5690566ca9f0965bc32
            constexpr u8 SectorWidth{16}; // The width of a sector in bytes
            constexpr u8 SectorHeight{2}; // The height of a sector in lines
            constexpr u8 GobWidth{64}; // The width of a GOB in bytes
            constexpr u8 GobHeight{8}; // The height of a GOB in lines

            auto blockHeight{guest->tileConfig.blockHeight}; // The height of the blocks in GOBs
            auto robHeight{GobHeight * blockHeight}; // The height of a single ROB (Row of Blocks) in lines
            auto surfaceHeight{dimensions.height / guest->format.blockHeight}; // The height of the surface in lines
            auto surfaceHeightRobs{util::AlignUp(surfaceHeight, robHeight) / robHeight}; // The height of the surface in ROBs (Row Of Blocks)
            auto robWidthBytes{util::AlignUp((guest->tileConfig.surfaceWidth / guest->format.blockWidth) * guest->format.bpb, GobWidth)}; // The width of a ROB in bytes
            auto robWidthBlocks{robWidthBytes / GobWidth}; // The width of a ROB in blocks (and GOBs because block width == 1 on the Tegra X1)
            auto robBytes{robWidthBytes * robHeight}; // The size of a ROB in bytes
            auto gobYOffset{robWidthBytes * GobHeight}; // The offset of the next Y-axis GOB from the current one in linear space

            auto inputSector{pointer}; // The address of the input sector
            auto outputRob{bufferData}; // The address of the output block

            for (u32 rob{}, y{}, paddingY{}; rob < surfaceHeightRobs; rob++) { // Every Surface contains `surfaceHeightRobs` ROBs
                auto outputBlock{outputRob}; // We iterate through a block independently of the ROB
                for (u32 block{}; block < robWidthBlocks; block++) { // Every ROB contains `surfaceWidthBlocks` Blocks
                    auto outputGob{outputBlock}; // We iterate through a GOB independently of the block
                    for (u32 gobY{}; gobY < blockHeight; gobY++) { // Every Block contains `blockHeight` Y-axis GOBs
                        for (u32 index{}; index < SectorWidth * SectorHeight; index++) { // Every Y-axis GOB contains `sectorWidth * sectorHeight` sectors
                            u32 xT{((index << 3) & 0b10000) | ((index << 1) & 0b100000)}; // Morton-Swizzle on the X-axis
                            u32 yT{((index >> 1) & 0b110) | (index & 0b1)}; // Morton-Swizzle on the Y-axis
                            std::memcpy(outputGob + (yT * robWidthBytes) + xT, inputSector, SectorWidth);
                            inputSector += SectorWidth; // `sectorWidth` bytes are of sequential image data
                        }
                        outputGob += gobYOffset; // Increment the output GOB to the next Y-axis GOB
                    }
                    inputSector += paddingY; // Increment the input sector to the next sector
                    outputBlock += GobWidth; // Increment the output block to the next block (As Block Width = 1 GOB Width)
                }
                outputRob += robBytes; // Increment the output block to the next ROB

                y += robHeight; // Increment the Y position to the next ROB
                blockHeight = static_cast<u8>(std::min(static_cast<u32>(blockHeight), (surfaceHeight - y) / GobHeight)); // Calculate the amount of Y GOBs which aren't padding
                paddingY = (guest->tileConfig.blockHeight - blockHeight) * (SectorWidth * SectorWidth * SectorHeight); // Calculate the amount of padding between contiguous sectors
            }
        } else if (guest->tileMode == texture::TileMode::Pitch) {
            auto sizeLine{guest->format.GetSize(dimensions.width, 1)}; // The size of a single line of pixel data
            auto sizeStride{guest->format.GetSize(guest->tileConfig.pitch, 1)}; // The size of a single stride of pixel data

            auto inputLine{pointer}; // The address of the input line
            auto outputLine{bufferData}; // The address of the output line

            for (u32 line{}; line < dimensions.height; line++) {
                std::memcpy(outputLine, inputLine, sizeLine);
                inputLine += sizeStride;
                outputLine += sizeLine;
            }
        } else if (guest->tileMode == texture::TileMode::Linear) {
            std::memcpy(bufferData, pointer, size);
        }

        if (stagingBuffer) {
            if (WaitOnBacking() && size != format.GetSize(dimensions))
                throw exception("Backing properties changing during sync is not supported");
            WaitOnFence();

            cycle = gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                auto image{GetBacking()};
                if (layout != vk::ImageLayout::eTransferDstOptimal) {
                    commandBuffer.pipelineBarrier(layout != vk::ImageLayout::eUndefined ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                        .image = image,
                        .srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                        .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                        .oldLayout = layout,
                        .newLayout = vk::ImageLayout::eTransferDstOptimal,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .subresourceRange = {
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .levelCount = 1,
                            .layerCount = 1,
                        },
                    });

                    if (layout == vk::ImageLayout::eUndefined)
                        layout = vk::ImageLayout::eTransferDstOptimal;
                }

                commandBuffer.copyBufferToImage(stagingBuffer->vkBuffer, image, vk::ImageLayout::eTransferDstOptimal, vk::BufferImageCopy{
                    .imageExtent = dimensions,
                    .imageSubresource = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .layerCount = 1,
                    },
                });

                if (layout != vk::ImageLayout::eTransferDstOptimal)
                    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                        .image = image,
                        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                        .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                        .newLayout = layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .subresourceRange = {
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .levelCount = 1,
                            .layerCount = 1,
                        },
                    });
            });
            cycle->AttachObjects(stagingBuffer, shared_from_this());
        }
    }

    void Texture::SynchronizeGuest() {
        if (!guest)
            throw exception("Synchronization of guest textures requires a valid guest texture to synchronize to");

        WaitOnBacking();
        WaitOnFence();

        TRACE_EVENT("gpu", "Texture::SynchronizeGuest");
        // TODO: Write Host -> Guest Synchronization
    }

    void Texture::CopyFrom(std::shared_ptr<Texture> source) {
        WaitOnBacking();
        WaitOnFence();

        source->WaitOnBacking();
        source->WaitOnFence();

        if (source->layout == vk::ImageLayout::eUndefined)
            throw exception("Cannot copy from image with undefined layout");
        else if (source->dimensions != dimensions)
            throw exception("Cannot copy from image with different dimensions");
        else if (source->format != format)
            throw exception("Cannot copy from image with different format");

        cycle = gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
            auto sourceBacking{source->GetBacking()};
            if (source->layout != vk::ImageLayout::eTransferSrcOptimal) {
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                    .image = sourceBacking,
                    .srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                    .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                    .oldLayout = source->layout,
                    .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });
            }

            auto destinationBacking{GetBacking()};
            if (layout != vk::ImageLayout::eTransferDstOptimal) {
                commandBuffer.pipelineBarrier(layout != vk::ImageLayout::eUndefined ? vk::PipelineStageFlagBits::eTopOfPipe : vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                    .image = destinationBacking,
                    .srcAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                    .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .oldLayout = layout,
                    .newLayout = vk::ImageLayout::eTransferDstOptimal,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });

                if (layout == vk::ImageLayout::eUndefined)
                    layout = vk::ImageLayout::eTransferDstOptimal;
            }

            commandBuffer.copyImage(sourceBacking, vk::ImageLayout::eTransferSrcOptimal, destinationBacking, vk::ImageLayout::eTransferDstOptimal, vk::ImageCopy{
                .srcSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .layerCount = 1,
                },
                .dstSubresource = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .layerCount = 1,
                },
                .extent = dimensions,
            });

            if (layout != vk::ImageLayout::eTransferDstOptimal)
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                    .image = destinationBacking,
                    .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                    .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                    .newLayout = layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });

            if (layout != vk::ImageLayout::eTransferSrcOptimal)
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                    .image = sourceBacking,
                    .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                    .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
                    .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                    .newLayout = source->layout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .subresourceRange = {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
                });
        });
        cycle->AttachObjects(source, shared_from_this());
    }
}
