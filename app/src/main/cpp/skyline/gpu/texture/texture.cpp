// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <common/trace.h>
#include <kernel/types/KProcess.h>
#include "texture.h"
#include "layout.h"

namespace skyline::gpu {
    u32 GuestTexture::GetLayerSize() {
        switch (tileConfig.mode) {
            case texture::TileMode::Linear:
                return layerStride = static_cast<u32>(format->GetSize(dimensions));

            case texture::TileMode::Pitch:
                return layerStride = dimensions.height * tileConfig.pitch;

            case texture::TileMode::Block:
                return layerStride = static_cast<u32>(texture::GetBlockLinearLayerSize(*this));
        }
    }

    TextureView::TextureView(std::shared_ptr<Texture> texture, vk::ImageViewType type, vk::ImageSubresourceRange range, texture::Format format, vk::ComponentMapping mapping) : texture(std::move(texture)), type(type), format(format), mapping(mapping), range(range) {}

    vk::ImageView TextureView::GetView() {
        if (view)
            return **view;

        vk::ImageViewCreateInfo createInfo{
            .image = texture->GetBacking(),
            .viewType = type,
            .format = format ? *format : *texture->format,
            .components = mapping,
            .subresourceRange = range,
        };

        return *view.emplace(texture->gpu.vkDevice, createInfo);
    }

    void TextureView::lock() {
        auto backing{std::atomic_load(&texture)};
        while (true) {
            backing->lock();

            auto latestBacking{std::atomic_load(&texture)};
            if (backing == latestBacking)
                return;

            backing->unlock();
            backing = latestBacking;
        }
    }

    void TextureView::unlock() {
        texture->unlock();
    }

    bool TextureView::try_lock() {
        auto backing{std::atomic_load(&texture)};
        while (true) {
            bool success{backing->try_lock()};

            auto latestBacking{std::atomic_load(&texture)};
            if (backing == latestBacking)
                // We want to ensure that the try_lock() was on the latest backing and not on an outdated one
                return success;

            if (success)
                // We only unlock() if the try_lock() was successful and we acquired the mutex
                backing->unlock();
            backing = latestBacking;
        }
    }

    void Texture::SetupGuestMappings() {
        auto &mappings{guest->mappings};
        if (mappings.size() == 1) {
            auto mapping{mappings.front()};
            u8 *alignedData{util::AlignDown(mapping.data(), PAGE_SIZE)};
            size_t alignedSize{static_cast<size_t>(util::AlignUp(mapping.data() + mapping.size(), PAGE_SIZE) - alignedData)};

            alignedMirror = gpu.state.process->memory.CreateMirror(alignedData, alignedSize);
            mirror = alignedMirror.subspan(static_cast<size_t>(mapping.data() - alignedData), mapping.size());
        } else {
            std::vector<span<u8>> alignedMappings;

            const auto &frontMapping{mappings.front()};
            u8 *alignedData{util::AlignDown(frontMapping.data(), PAGE_SIZE)};
            alignedMappings.emplace_back(alignedData, (frontMapping.data() + frontMapping.size()) - alignedData);

            size_t totalSize{frontMapping.size()};
            for (auto it{std::next(mappings.begin())}; it != std::prev(mappings.end()); ++it) {
                auto mappingSize{it->size()};
                alignedMappings.emplace_back(it->data(), mappingSize);
                totalSize += mappingSize;
            }

            const auto &backMapping{mappings.back()};
            totalSize += backMapping.size();
            alignedMappings.emplace_back(backMapping.data(), util::AlignUp(backMapping.size(), PAGE_SIZE));

            alignedMirror = gpu.state.process->memory.CreateMirrors(alignedMappings);
            mirror = alignedMirror.subspan(static_cast<size_t>(frontMapping.data() - alignedData), totalSize);
        }

        trapHandle = gpu.state.nce->TrapRegions(mappings, true, [this] {
            std::lock_guard lock(*this);
            SynchronizeGuest(true); // We can skip trapping since the caller will do it
            WaitOnFence();
        }, [this] {
            std::lock_guard lock(*this);
            SynchronizeGuest(true);
            dirtyState = DirtyState::CpuDirty; // We need to assume the texture is dirty since we don't know what the guest is writing
            WaitOnFence();
        });
    }

    std::shared_ptr<memory::StagingBuffer> Texture::SynchronizeHostImpl(const std::shared_ptr<FenceCycle> &pCycle) {
        if (!guest)
            throw exception("Synchronization of host textures requires a valid guest texture to synchronize from");
        else if (guest->dimensions != dimensions)
            throw exception("Guest and host dimensions being different is not supported currently");

        auto pointer{mirror.data()};
        auto size{format->GetSize(dimensions) * layerCount};

        WaitOnBacking();

        u8 *bufferData;
        auto stagingBuffer{[&]() -> std::shared_ptr<memory::StagingBuffer> {
            if (tiling == vk::ImageTiling::eOptimal || !std::holds_alternative<memory::Image>(backing)) {
                // We need a staging buffer for all optimal copies (since we aren't aware of the host optimal layout) and linear textures which we cannot map on the CPU since we do not have access to their backing VkDeviceMemory
                auto stagingBuffer{gpu.memory.AllocateStagingBuffer(size)};
                bufferData = stagingBuffer->data();
                return stagingBuffer;
            } else if (tiling == vk::ImageTiling::eLinear) {
                // We can optimize linear texture sync on a UMA by mapping the texture onto the CPU and copying directly into it rather than a staging buffer
                if (layout == vk::ImageLayout::eUndefined)
                    TransitionLayout(vk::ImageLayout::eGeneral);
                bufferData = std::get<memory::Image>(backing).data();
                if (cycle.lock() != pCycle)
                    WaitOnFence();
                return nullptr;
            } else {
                throw exception("Guest -> Host synchronization of images tiled as '{}' isn't implemented", vk::to_string(tiling));
            }
        }()};

        if (guest->tileConfig.mode == texture::TileMode::Block)
            texture::CopyBlockLinearToLinear(*guest, pointer, bufferData);
        else if (guest->tileConfig.mode == texture::TileMode::Pitch)
            texture::CopyPitchLinearToLinear(*guest, pointer, bufferData);
        else if (guest->tileConfig.mode == texture::TileMode::Linear)
            std::memcpy(bufferData, pointer, size);

        if (stagingBuffer && cycle.lock() != pCycle)
            WaitOnFence();

        return stagingBuffer;
    }

    void Texture::CopyFromStagingBuffer(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<memory::StagingBuffer> &stagingBuffer) {
        auto image{GetBacking()};
        if (layout == vk::ImageLayout::eUndefined)
            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                .image = image,
                .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = std::exchange(layout, vk::ImageLayout::eGeneral),
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .subresourceRange = {
                    .aspectMask = format->vkAspect,
                    .levelCount = mipLevels,
                    .layerCount = layerCount,
                },
            });

        boost::container::static_vector<const vk::BufferImageCopy, 3> bufferImageCopies;
        auto pushBufferImageCopyWithAspect{[&](vk::ImageAspectFlagBits aspect) {
            bufferImageCopies.emplace_back(
                vk::BufferImageCopy{
                    .imageExtent = dimensions,
                    .imageSubresource = {
                        .aspectMask = aspect,
                        .layerCount = layerCount,
                    },
                });
        }};

        if (format->vkAspect & vk::ImageAspectFlagBits::eColor)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eColor);
        if (format->vkAspect & vk::ImageAspectFlagBits::eDepth)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eDepth);
        if (format->vkAspect & vk::ImageAspectFlagBits::eStencil)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eStencil);

        commandBuffer.copyBufferToImage(stagingBuffer->vkBuffer, image, layout, vk::ArrayProxy(static_cast<u32>(bufferImageCopies.size()), bufferImageCopies.data()));
    }

    void Texture::CopyIntoStagingBuffer(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<memory::StagingBuffer> &stagingBuffer) {
        auto image{GetBacking()};
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
            .image = image,
            .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = layout,
            .newLayout = layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .subresourceRange = {
                .aspectMask = format->vkAspect,
                .levelCount = mipLevels,
                .layerCount = layerCount,
            },
        });

        boost::container::static_vector<const vk::BufferImageCopy, 3> bufferImageCopies;
        auto pushBufferImageCopyWithAspect{[&](vk::ImageAspectFlagBits aspect) {
            bufferImageCopies.emplace_back(
                vk::BufferImageCopy{
                    .imageExtent = dimensions,
                    .imageSubresource = {
                        .aspectMask = aspect,
                        .layerCount = layerCount,
                    },
                });
        }};

        if (format->vkAspect & vk::ImageAspectFlagBits::eColor)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eColor);
        if (format->vkAspect & vk::ImageAspectFlagBits::eDepth)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eDepth);
        if (format->vkAspect & vk::ImageAspectFlagBits::eStencil)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eStencil);

        commandBuffer.copyImageToBuffer(image, layout, stagingBuffer->vkBuffer, vk::ArrayProxy(static_cast<u32>(bufferImageCopies.size()), bufferImageCopies.data()));

        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost, {}, {}, vk::BufferMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = vk::AccessFlagBits::eHostRead,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = stagingBuffer->vkBuffer,
            .offset = 0,
            .size = stagingBuffer->size(),
        }, {});
    }

    void Texture::CopyToGuest(u8 *hostBuffer) {
        auto guestOutput{mirror.data()};

        if (guest->tileConfig.mode == texture::TileMode::Block)
            texture::CopyLinearToBlockLinear(*guest, hostBuffer, guestOutput);
        else if (guest->tileConfig.mode == texture::TileMode::Pitch)
            texture::CopyLinearToPitchLinear(*guest, hostBuffer, guestOutput);
        else if (guest->tileConfig.mode == texture::TileMode::Linear)
            std::memcpy(hostBuffer, guestOutput, format->GetSize(dimensions));
    }

    Texture::TextureBufferCopy::TextureBufferCopy(std::shared_ptr<Texture> texture, std::shared_ptr<memory::StagingBuffer> stagingBuffer) : texture(std::move(texture)), stagingBuffer(std::move(stagingBuffer)) {}

    Texture::TextureBufferCopy::~TextureBufferCopy() {
        TRACE_EVENT("gpu", "Texture::TextureBufferCopy");
        texture->CopyToGuest(stagingBuffer ? stagingBuffer->data() : std::get<memory::Image>(texture->backing).data());
    }

    Texture::Texture(GPU &gpu, BackingType &&backing, texture::Dimensions dimensions, texture::Format format, vk::ImageLayout layout, vk::ImageTiling tiling, u32 mipLevels, u32 layerCount, vk::SampleCountFlagBits sampleCount)
        : gpu(gpu),
          backing(std::move(backing)),
          dimensions(dimensions),
          format(format),
          layout(layout),
          tiling(tiling),
          mipLevels(mipLevels),
          layerCount(layerCount),
          sampleCount(sampleCount) {}

    Texture::Texture(GPU &pGpu, GuestTexture pGuest)
        : gpu(pGpu),
          guest(std::move(pGuest)),
          dimensions(guest->dimensions),
          format(guest->format),
          layout(vk::ImageLayout::eUndefined),
          tiling(vk::ImageTiling::eOptimal), // Force Optimal due to not adhering to host subresource layout during Linear synchronization
          mipLevels(1),
          layerCount(guest->layerCount),
          sampleCount(vk::SampleCountFlagBits::e1) {
        vk::ImageUsageFlags usage{vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled};
        if ((format->vkAspect & vk::ImageAspectFlagBits::eColor) && !format->IsCompressed())
            usage |= vk::ImageUsageFlagBits::eColorAttachment;
        if (format->vkAspect & (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil))
            usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;

        //  First attempt to derive type from dimensions
        auto imageType{guest->dimensions.GetType()};

        // Try to ensure that the image type is compatible with the given image view type since we can't create a 2D image view from a 1D image
        if (imageType == vk::ImageType::e1D && guest->type != texture::TextureType::e1D && guest->type != texture::TextureType::e1DArray) {
            switch (guest->type) {
                case texture::TextureType::e3D:
                    imageType = vk::ImageType::e3D;
                    break;
                default:
                    imageType = vk::ImageType::e2D;
                    break;
            }
        }

        vk::ImageCreateFlags flags{gpu.traits.quirks.vkImageMutableFormatCostly ? vk::ImageCreateFlags{} : vk::ImageCreateFlagBits::eMutableFormat};

        if (imageType == vk::ImageType::e2D && dimensions.width == dimensions.height && layerCount >= 6)
            flags |= vk::ImageCreateFlagBits::eCubeCompatible;
        else if (imageType == vk::ImageType::e3D)
            flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;


        vk::ImageCreateInfo imageCreateInfo{
            .flags = flags,
            .imageType = imageType,
            .format = *guest->format,
            .extent = guest->dimensions,
            .mipLevels = 1,
            .arrayLayers = guest->layerCount,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &gpu.vkQueueFamilyIndex,
            .initialLayout = layout,
        };
        backing = tiling != vk::ImageTiling::eLinear ? gpu.memory.AllocateImage(imageCreateInfo) : gpu.memory.AllocateMappedImage(imageCreateInfo);

        SetupGuestMappings();
    }

    Texture::~Texture() {
        std::lock_guard lock(*this);
        if (trapHandle)
            gpu.state.nce->DeleteTrap(*trapHandle);
        SynchronizeGuest(true);
        if (alignedMirror.valid())
            munmap(alignedMirror.data(), alignedMirror.size());
    }

    void Texture::MarkGpuDirty() {
        if (dirtyState == DirtyState::GpuDirty)
            return;
        gpu.state.nce->RetrapRegions(*trapHandle, false);
        dirtyState = DirtyState::GpuDirty;
    }

    bool Texture::WaitOnBacking() {
        TRACE_EVENT("gpu", "Texture::WaitOnBacking");

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
        TRACE_EVENT("gpu", "Texture::WaitOnFence");

        auto lCycle{cycle.lock()};
        if (lCycle) {
            lCycle->Wait();
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

        TRACE_EVENT("gpu", "Texture::TransitionLayout");

        if (layout != pLayout) {
            auto lCycle{gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, vk::ImageMemoryBarrier{
                    .image = GetBacking(),
                    .srcAccessMask = vk::AccessFlagBits::eNoneKHR,
                    .dstAccessMask = vk::AccessFlagBits::eNoneKHR,
                    .oldLayout = std::exchange(layout, pLayout),
                    .newLayout = pLayout,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .subresourceRange = {
                        .aspectMask = format->vkAspect,
                        .levelCount = mipLevels,
                        .layerCount = layerCount,
                    },
                });
            })};
            lCycle->AttachObject(shared_from_this());
            cycle = lCycle;
        }
    }

    void Texture::SynchronizeHost(bool rwTrap) {
        if (dirtyState != DirtyState::CpuDirty)
            return; // If the texture has not been modified on the CPU, there is no need to synchronize it

        TRACE_EVENT("gpu", "Texture::SynchronizeHost");

        auto stagingBuffer{SynchronizeHostImpl(nullptr)};
        if (stagingBuffer) {
            auto lCycle{gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                CopyFromStagingBuffer(commandBuffer, stagingBuffer);
            })};
            lCycle->AttachObjects(stagingBuffer, shared_from_this());
            cycle = lCycle;
        }

        if (rwTrap) {
            gpu.state.nce->RetrapRegions(*trapHandle, false);
            dirtyState = DirtyState::GpuDirty;
        } else {
            gpu.state.nce->RetrapRegions(*trapHandle, true); // Trap any future CPU writes to this texture
            dirtyState = DirtyState::Clean;
        }
    }

    void Texture::SynchronizeHostWithBuffer(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &pCycle, bool rwTrap) {
        if (dirtyState != DirtyState::CpuDirty)
            return;

        TRACE_EVENT("gpu", "Texture::SynchronizeHostWithBuffer");

        auto stagingBuffer{SynchronizeHostImpl(pCycle)};
        if (stagingBuffer) {
            CopyFromStagingBuffer(commandBuffer, stagingBuffer);
            pCycle->AttachObjects(stagingBuffer, shared_from_this());
            cycle = pCycle;
        }

        if (rwTrap) {
            gpu.state.nce->RetrapRegions(*trapHandle, false);
            dirtyState = DirtyState::GpuDirty;
        } else {
            gpu.state.nce->RetrapRegions(*trapHandle, true); // Trap any future CPU writes to this texture
            dirtyState = DirtyState::Clean;
        }
    }

    void Texture::SynchronizeGuest(bool skipTrap) {
        if (dirtyState != DirtyState::GpuDirty || layout == vk::ImageLayout::eUndefined) {
            // We can skip syncing in two cases:
            // * If the texture has not been used on the GPU, there is no need to synchronize it
            // * If the state of the host texture is undefined then so can the guest
            return;
        } else if (!guest) {
            throw exception("Synchronization of guest textures requires a valid guest texture to synchronize to");
        }

        TRACE_EVENT("gpu", "Texture::SynchronizeGuest");

        WaitOnBacking();
        WaitOnFence();

        if (tiling == vk::ImageTiling::eOptimal || !std::holds_alternative<memory::Image>(backing)) {
            auto size{format->GetSize(dimensions)};
            auto stagingBuffer{gpu.memory.AllocateStagingBuffer(size)};

            auto lCycle{gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                CopyIntoStagingBuffer(commandBuffer, stagingBuffer);
            })};
            lCycle->AttachObject(std::make_shared<TextureBufferCopy>(shared_from_this(), stagingBuffer));
            cycle = lCycle;
        } else if (tiling == vk::ImageTiling::eLinear) {
            // We can optimize linear texture sync on a UMA by mapping the texture onto the CPU and copying directly from it rather than using a staging buffer
            CopyToGuest(std::get<memory::Image>(backing).data());
        } else {
            throw exception("Host -> Guest synchronization of images tiled as '{}' isn't implemented", vk::to_string(tiling));
        }

        if (!skipTrap)
            gpu.state.nce->RetrapRegions(*trapHandle, true);
        dirtyState = DirtyState::Clean;
    }

    void Texture::SynchronizeGuestWithBuffer(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &pCycle) {
        if (dirtyState != DirtyState::GpuDirty)
            return;

        if (!guest)
            throw exception("Synchronization of guest textures requires a valid guest texture to synchronize to");
        else if (layout == vk::ImageLayout::eUndefined)
            return; // If the state of the host texture is undefined then so can the guest

        TRACE_EVENT("gpu", "Texture::SynchronizeGuestWithBuffer");

        WaitOnBacking();
        if (cycle.lock() != pCycle)
            WaitOnFence();

        if (tiling == vk::ImageTiling::eOptimal || !std::holds_alternative<memory::Image>(backing)) {
            auto size{format->GetSize(dimensions)};
            auto stagingBuffer{gpu.memory.AllocateStagingBuffer(size)};

            CopyIntoStagingBuffer(commandBuffer, stagingBuffer);
            pCycle->AttachObject(std::make_shared<TextureBufferCopy>(shared_from_this(), stagingBuffer));
            cycle = pCycle;
        } else if (tiling == vk::ImageTiling::eLinear) {
            CopyToGuest(std::get<memory::Image>(backing).data());
            pCycle->AttachObject(std::make_shared<TextureBufferCopy>(shared_from_this()));
            cycle = pCycle;
        } else {
            throw exception("Host -> Guest synchronization of images tiled as '{}' isn't implemented", vk::to_string(tiling));
        }

        dirtyState = DirtyState::Clean;
    }

    std::shared_ptr<TextureView> Texture::GetView(vk::ImageViewType type, vk::ImageSubresourceRange range, texture::Format pFormat, vk::ComponentMapping mapping) {
        for (auto viewIt{views.begin()}; viewIt != views.end();) {
            auto view{viewIt->lock()};
            if (view && type == view->type && pFormat == view->format && range == view->range && mapping == view->mapping)
                return view;
            else if (!view)
                viewIt = views.erase(viewIt);
            else
                ++viewIt;
        }

        if (gpu.traits.quirks.vkImageMutableFormatCostly && pFormat->vkFormat != format->vkFormat)
            Logger::Warn("Creating a view of a texture with a different format without mutable format: {} - {}", vk::to_string(pFormat->vkFormat), vk::to_string(format->vkFormat));

        auto view{std::make_shared<TextureView>(shared_from_this(), type, range, pFormat, mapping)};
        views.push_back(view);
        return view;
    }

    void Texture::CopyFrom(std::shared_ptr<Texture> source, const vk::ImageSubresourceRange &subresource) {
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

        TRACE_EVENT("gpu", "Texture::CopyFrom");

        auto lCycle{gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
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
                    .subresourceRange = subresource,
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
                    .subresourceRange = subresource,
                });

                if (layout == vk::ImageLayout::eUndefined)
                    layout = vk::ImageLayout::eTransferDstOptimal;
            }

            vk::ImageSubresourceLayers subresourceLayers{
                .aspectMask = subresource.aspectMask,
                .mipLevel = subresource.baseMipLevel,
                .baseArrayLayer = subresource.baseArrayLayer,
                .layerCount = subresource.layerCount == VK_REMAINING_ARRAY_LAYERS ? layerCount - subresource.baseArrayLayer : subresource.layerCount,
            };
            for (; subresourceLayers.mipLevel < (subresource.levelCount == VK_REMAINING_MIP_LEVELS ? mipLevels - subresource.baseMipLevel : subresource.levelCount); subresourceLayers.mipLevel++)
                commandBuffer.copyImage(sourceBacking, vk::ImageLayout::eTransferSrcOptimal, destinationBacking, vk::ImageLayout::eTransferDstOptimal, vk::ImageCopy{
                    .srcSubresource = subresourceLayers,
                    .dstSubresource = subresourceLayers,
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
                    .subresourceRange = subresource,
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
                    .subresourceRange = subresource,
                });
        })};
        lCycle->AttachObjects(std::move(source), shared_from_this());
        cycle = lCycle;
    }
}
