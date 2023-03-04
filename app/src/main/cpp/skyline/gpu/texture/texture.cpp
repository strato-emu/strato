// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/memory.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include <common/settings.h>
#include "texture.h"
#include "layout.h"
#include "adreno_aliasing.h"
#include "bc_decoder.h"
#include "format.h"

namespace skyline::gpu {
    u32 GuestTexture::GetLayerStride() {
        if (layerStride)
            return layerStride;

        layerStride = CalculateLayerSize();
        return layerStride;
    }

    u32 GuestTexture::CalculateLayerSize() const {
        switch (tileConfig.mode) {
            case texture::TileMode::Linear:
                return static_cast<u32>(format->GetSize(dimensions));

            case texture::TileMode::Pitch:
                return dimensions.height * tileConfig.pitch;

            case texture::TileMode::Block:
                return static_cast<u32>(texture::GetBlockLinearLayerSize(dimensions, format->blockHeight, format->blockWidth, format->bpb, tileConfig.blockHeight, tileConfig.blockDepth, mipLevelCount, layerCount > 1));
        }
    }

    vk::ImageType GuestTexture::GetImageType() const {
        switch (viewType) {
            case vk::ImageViewType::e1D:
            case vk::ImageViewType::e1DArray:
                return vk::ImageType::e1D;
            case vk::ImageViewType::e2D:
            case vk::ImageViewType::e2DArray:
                // If depth is > 1 this is a 2D view into a 3D texture so the underlying image needs to be created as 3D yoo
                if (dimensions.depth > 1)
                    return vk::ImageType::e3D;
                else
                    return vk::ImageType::e2D;
            case vk::ImageViewType::eCube:
            case vk::ImageViewType::eCubeArray:
                return vk::ImageType::e2D;
            case vk::ImageViewType::e3D:
                return vk::ImageType::e3D;
        }
    }

    u32 GuestTexture::GetViewLayerCount() const {
        if (GetImageType() == vk::ImageType::e3D && viewType != vk::ImageViewType::e3D)
            return dimensions.depth;
        else
            return layerCount;
    }

    u32 GuestTexture::GetViewDepth() const {
        if (GetImageType() == vk::ImageType::e3D && viewType != vk::ImageViewType::e3D)
            return layerCount;
        else
            return dimensions.depth;
    }

    size_t GuestTexture::GetSize() {
        return GetLayerStride() * (layerCount - baseArrayLayer);
    }

    bool GuestTexture::MappingsValid() const {
        return ranges::all_of(mappings, [](const auto &mapping) { return mapping.valid(); });
    }

    TextureView::TextureView(std::shared_ptr<Texture> texture, vk::ImageViewType type, vk::ImageSubresourceRange range, texture::Format format, vk::ComponentMapping mapping) : texture(std::move(texture)), type(type), format(format), mapping(mapping), range(range) {}

    Texture::TextureViewStorage::TextureViewStorage(vk::ImageViewType type, texture::Format format, vk::ComponentMapping mapping, vk::ImageSubresourceRange range, vk::raii::ImageView &&vkView) : type(type), format(format), mapping(mapping), range(range), vkView(std::move(vkView)) {}

    vk::ImageView TextureView::GetView() {
        if (vkView)
            return vkView;

        auto it{std::find_if(texture->views.begin(), texture->views.end(), [this](const Texture::TextureViewStorage &view) {
            return view.type == type && view.format == format && view.mapping == mapping && view.range == range;
        })};
        if (it == texture->views.end()) {
            vk::ImageViewCreateInfo createInfo{
                .image = texture->GetBacking(),
                .viewType = type,
                .format = format ? *format : *texture->format,
                .components = mapping,
                .subresourceRange = range,
            };

            it = texture->views.emplace(texture->views.end(), type, format, mapping, range, vk::raii::ImageView{texture->gpu.vkDevice, createInfo});
        }

        return vkView = *it->vkView;
    }

    void TextureView::lock() {
        texture.Lock();
    }

    bool TextureView::LockWithTag(ContextTag tag) {
        bool result{};
        texture.Lock([tag, &result](Texture *pTexture) {
            result = pTexture->LockWithTag(tag);
        });
        return result;
    }

    void TextureView::unlock() {
        texture->unlock();
    }

    bool TextureView::try_lock() {
        return texture.TryLock();
    }

    void Texture::SetupGuestMappings() {
        auto &mappings{guest->mappings};
        if (mappings.size() == 1) {
            auto mapping{mappings.front()};
            u8 *alignedData{util::AlignDown(mapping.data(), constant::PageSize)};
            size_t alignedSize{static_cast<size_t>(util::AlignUp(mapping.data() + mapping.size(), constant::PageSize) - alignedData)};

            alignedMirror = gpu.state.process->memory.CreateMirror(span<u8>{alignedData, alignedSize});
            mirror = alignedMirror.subspan(static_cast<size_t>(mapping.data() - alignedData), mapping.size());
        } else {
            std::vector<span<u8>> alignedMappings;

            const auto &frontMapping{mappings.front()};
            u8 *alignedData{util::AlignDown(frontMapping.data(), constant::PageSize)};
            alignedMappings.emplace_back(alignedData, (frontMapping.data() + frontMapping.size()) - alignedData);

            size_t totalSize{frontMapping.size()};
            for (auto it{std::next(mappings.begin())}; it != std::prev(mappings.end()); ++it) {
                auto mappingSize{it->size()};
                alignedMappings.emplace_back(it->data(), mappingSize);
                totalSize += mappingSize;
            }

            const auto &backMapping{mappings.back()};
            totalSize += backMapping.size();
            alignedMappings.emplace_back(backMapping.data(), util::AlignUp(backMapping.size(), constant::PageSize));

            alignedMirror = gpu.state.process->memory.CreateMirrors(alignedMappings);
            mirror = alignedMirror.subspan(static_cast<size_t>(frontMapping.data() - alignedData), totalSize);
        }

        // We can't just capture `this` in the lambda since the lambda could exceed the lifetime of the buffer
        std::weak_ptr<Texture> weakThis{weak_from_this()};
        trapHandle = gpu.state.nce->CreateTrap(mappings, [weakThis] {
            auto texture{weakThis.lock()};
            if (!texture)
                return;

            std::unique_lock stateLock{texture->stateMutex};
            if (texture->dirtyState == DirtyState::GpuDirty) {
                stateLock.unlock(); // If the lock isn't unlocked, a deadlock from threads waiting on the other lock can occur

                // If this mutex would cause other callbacks to be blocked then we should block on this mutex in advance
                std::shared_ptr<FenceCycle> waitCycle{};
                do {
                    // We need to do a loop here since we can't wait with the texture locked but not doing so means that the texture could have it's cycle changed which we wouldn't wait on, loop until we are sure the cycle hasn't changed to avoid that
                    if (waitCycle) {
                        i64 startNs{texture->accumulatedGuestWaitCounter > SkipReadbackHackWaitCountThreshold ? util::GetTimeNs() : 0};
                        waitCycle->Wait();
                        if (startNs)
                            texture->accumulatedGuestWaitTime += std::chrono::nanoseconds(util::GetTimeNs() - startNs);

                        texture->accumulatedGuestWaitCounter++;
                    }

                    std::scoped_lock lock{*texture};
                    if (waitCycle && texture->cycle == waitCycle) {
                        texture->cycle = {};
                        waitCycle = {};
                    } else {
                        waitCycle = texture->cycle;
                    }
                } while (waitCycle);
            }
        }, [weakThis] {
            TRACE_EVENT("gpu", "Texture::ReadTrap");

            auto texture{weakThis.lock()};
            if (!texture)
                return true;

            std::unique_lock stateLock{texture->stateMutex, std::try_to_lock};
            if (!stateLock)
                return false;

            if (texture->dirtyState != DirtyState::GpuDirty)
                return true; // If state is already CPU dirty/Clean we don't need to do anything

            std::unique_lock lock{*texture, std::try_to_lock};
            if (!lock)
                return false;

            if (texture->cycle)
                return false;

            texture->SynchronizeGuest(false, true); // We can skip trapping since the caller will do it
            return true;
        }, [weakThis] {
            TRACE_EVENT("gpu", "Texture::WriteTrap");

            auto texture{weakThis.lock()};
            if (!texture)
                return true;

            std::unique_lock stateLock{texture->stateMutex, std::try_to_lock};
            if (!stateLock)
                return false;

            if (texture->dirtyState != DirtyState::GpuDirty) {
                texture->dirtyState = DirtyState::CpuDirty;
                return true; // If the texture is already CPU dirty or we can transition it to being CPU dirty then we don't need to do anything
            }

            if (texture->accumulatedGuestWaitTime > SkipReadbackHackWaitTimeThreshold && *texture->gpu.state.settings->enableFastGpuReadbackHack && !texture->memoryFreed) {
                texture->dirtyState = DirtyState::Clean;
                return true;
            }

            std::unique_lock lock{*texture, std::try_to_lock};
            if (!lock)
                return false;

            if (texture->cycle)
                return false;

            texture->SynchronizeGuest(true, true); // We need to assume the texture is dirty since we don't know what the guest is writing
            return true;
        });
    }

    std::shared_ptr<memory::StagingBuffer> Texture::SynchronizeHostImpl() {
        if (guest->dimensions != dimensions)
            throw exception("Guest and host dimensions being different is not supported currently");

        auto pointer{mirror.data()};

        WaitOnBacking();

        u8 *bufferData;
        auto stagingBuffer{[&]() -> std::shared_ptr<memory::StagingBuffer> {
            if (tiling == vk::ImageTiling::eOptimal || !std::holds_alternative<memory::Image>(backing)) {
                // We need a staging buffer for all optimal copies (since we aren't aware of the host optimal layout) and linear textures which we cannot map on the CPU since we do not have access to their backing VkDeviceMemory
                auto stagingBuffer{gpu.memory.AllocateStagingBuffer(surfaceSize)};
                bufferData = stagingBuffer->data();
                return stagingBuffer;
            } else if (tiling == vk::ImageTiling::eLinear) {
                // We can optimize linear texture sync on a UMA by mapping the texture onto the CPU and copying directly into it rather than a staging buffer
                if (layout == vk::ImageLayout::eUndefined)
                    TransitionLayout(vk::ImageLayout::eGeneral);
                bufferData = std::get<memory::Image>(backing).data();
                WaitOnFence();
                return nullptr;
            } else {
                throw exception("Guest -> Host synchronization of images tiled as '{}' isn't implemented", vk::to_string(tiling));
            }
        }()};

        std::vector<u8> deswizzleBuffer;
        u8 *deswizzleOutput;
        if (guest->format != format) {
            deswizzleBuffer.resize(deswizzledSurfaceSize);
            deswizzleOutput = deswizzleBuffer.data();
        } else [[likely]] {
            deswizzleOutput = bufferData;
        }

        auto guestLayerStride{guest->GetLayerStride()};
        if (levelCount == 1) {
            auto outputLayer{deswizzleOutput};
            for (size_t layer{}; layer < layerCount; layer++) {
                if (guest->tileConfig.mode == texture::TileMode::Block)
                    texture::CopyBlockLinearToLinear(*guest, pointer, outputLayer);
                else if (guest->tileConfig.mode == texture::TileMode::Pitch)
                    texture::CopyPitchLinearToLinear(*guest, pointer, outputLayer);
                else if (guest->tileConfig.mode == texture::TileMode::Linear)
                    std::memcpy(outputLayer, pointer, surfaceSize);
                pointer += guestLayerStride;
                outputLayer += deswizzledLayerStride;
            }
        } else if (levelCount > 1 && guest->tileConfig.mode == texture::TileMode::Block) {
            // We need to generate a buffer that has all layers for a given mip level while Tegra X1 layout holds all mip levels for a given layer
            for (size_t layer{}; layer < layerCount; layer++) {
                auto inputLevel{pointer}, outputLevel{deswizzleOutput};
                for (const auto &level : mipLayouts) {
                    texture::CopyBlockLinearToLinear(
                        level.dimensions,
                        guest->format->blockWidth, guest->format->blockHeight, guest->format->bpb,
                        level.blockHeight, level.blockDepth,
                        inputLevel, outputLevel + (layer * level.linearSize) // Offset into the current layer relative to the start of the current mip level
                    );

                    inputLevel += level.blockLinearSize; // Skip over the current mip level as we've deswizzled it
                    outputLevel += layerCount * level.linearSize; // We need to offset the output buffer by the size of the previous mip level
                }

                pointer += guestLayerStride; // We need to offset the input buffer by the size of the previous guest layer, this can differ from inputLevel's value due to layer end padding or guest RT layer stride
            }
        } else if (levelCount != 0) {
            throw exception("Mipmapped textures with tiling mode '{}' aren't supported", static_cast<int>(tiling));
        }

        if (!deswizzleBuffer.empty()) {
            for (const auto &level : mipLayouts) {
                size_t levelHeight{level.dimensions.height * layerCount}; //!< The height of an image representing all layers in the entire level
                switch (guest->format->vkFormat) {
                    case vk::Format::eBc1RgbaUnormBlock:
                    case vk::Format::eBc1RgbaSrgbBlock:
                        bcn::DecodeBc1(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, true);
                        break;

                    case vk::Format::eBc2UnormBlock:
                    case vk::Format::eBc2SrgbBlock:
                        bcn::DecodeBc2(deswizzleOutput, bufferData, level.dimensions.width, levelHeight);
                        break;

                    case vk::Format::eBc3UnormBlock:
                    case vk::Format::eBc3SrgbBlock:
                        bcn::DecodeBc3(deswizzleOutput, bufferData, level.dimensions.width, levelHeight);
                        break;

                    case vk::Format::eBc4UnormBlock:
                        bcn::DecodeBc4(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, false);
                        break;
                    case vk::Format::eBc4SnormBlock:
                        bcn::DecodeBc4(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, true);
                        break;

                    case vk::Format::eBc5UnormBlock:
                        bcn::DecodeBc5(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, false);
                        break;
                    case vk::Format::eBc5SnormBlock:
                        bcn::DecodeBc5(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, true);
                        break;

                    case vk::Format::eBc6HUfloatBlock:
                        bcn::DecodeBc6(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, false);
                        break;
                    case vk::Format::eBc6HSfloatBlock:
                        bcn::DecodeBc6(deswizzleOutput, bufferData, level.dimensions.width, levelHeight, true);
                        break;

                    case vk::Format::eBc7UnormBlock:
                    case vk::Format::eBc7SrgbBlock:
                        bcn::DecodeBc7(deswizzleOutput, bufferData, level.dimensions.width, levelHeight);
                        break;

                    default:
                        throw exception("Unsupported guest format '{}'", vk::to_string(guest->format->vkFormat));
                }

                deswizzleOutput += level.linearSize * layerCount;
                bufferData += level.targetLinearSize * layerCount;
            }
        }

        return stagingBuffer;
    }

    boost::container::small_vector<vk::BufferImageCopy, 10> Texture::GetBufferImageCopies() {
        boost::container::small_vector<vk::BufferImageCopy, 10> bufferImageCopies;

        auto pushBufferImageCopyWithAspect{[&](vk::ImageAspectFlagBits aspect) {
            vk::DeviceSize bufferOffset{};
            u32 mipLevel{};
            for (auto &level : mipLayouts) {
                bufferImageCopies.emplace_back(
                    vk::BufferImageCopy{
                        .bufferOffset = bufferOffset,
                        .imageSubresource = {
                            .aspectMask = aspect,
                            .mipLevel = mipLevel++,
                            .layerCount = layerCount,
                        },
                        .imageExtent = level.dimensions,
                    }
                );
                bufferOffset += level.targetLinearSize * layerCount;
            }
        }};

        if (format->vkAspect & vk::ImageAspectFlagBits::eColor)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eColor);
        if (format->vkAspect & vk::ImageAspectFlagBits::eDepth)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eDepth);
        if (format->vkAspect & vk::ImageAspectFlagBits::eStencil)
            pushBufferImageCopyWithAspect(vk::ImageAspectFlagBits::eStencil);

        return bufferImageCopies;
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
                    .levelCount = levelCount,
                    .layerCount = layerCount,
                },
            });

        auto bufferImageCopies{GetBufferImageCopies()};
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
                .levelCount = levelCount,
                .layerCount = layerCount,
            },
        });

        auto bufferImageCopies{GetBufferImageCopies()};
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

        auto guestLayerStride{guest->GetLayerStride()};
        if (levelCount == 1) {
            for (size_t layer{}; layer < layerCount; layer++) {
                if (guest->tileConfig.mode == texture::TileMode::Block)
                    texture::CopyLinearToBlockLinear(*guest, hostBuffer, guestOutput);
                else if (guest->tileConfig.mode == texture::TileMode::Pitch)
                    texture::CopyLinearToPitchLinear(*guest, hostBuffer, guestOutput);
                else if (guest->tileConfig.mode == texture::TileMode::Linear)
                    std::memcpy(hostBuffer, guestOutput, layerStride);
                guestOutput += guestLayerStride;
                hostBuffer += layerStride;
            }
        } else if (levelCount > 1 && guest->tileConfig.mode == texture::TileMode::Block) {
            // We need to copy into the Tegra X1 layout holds all mip levels for a given layer while the input buffer has all layers for a given mip level
            // Note: See SynchronizeHostImpl for additional comments
            for (size_t layer{}; layer < layerCount; layer++) {
                auto outputLevel{guestOutput}, inputLevel{hostBuffer};
                for (const auto &level : mipLayouts) {
                    texture::CopyLinearToBlockLinear(
                        level.dimensions,
                        guest->format->blockWidth, guest->format->blockHeight, guest->format->bpb,
                        level.blockHeight, level.blockDepth,
                        inputLevel + (layer * level.linearSize), outputLevel
                    );

                    outputLevel += level.blockLinearSize;
                    inputLevel += layerCount * level.linearSize;
                }

                guestOutput += guestLayerStride;
            }
        } else if (levelCount != 0) {
            throw exception("Mipmapped textures with tiling mode '{}' aren't supported", static_cast<int>(tiling));
        }
    }

    void Texture::FreeGuest() {
        // Avoid freeing memory if the backing format doesn't match, as otherwise texture data would be lost on the guest side, also avoid if fast readback is active
        if (*gpu.state.settings->freeGuestTextureMemory && guest->format == format && !(accumulatedGuestWaitTime > SkipReadbackHackWaitTimeThreshold && *gpu.state.settings->enableFastGpuReadbackHack)) {
            gpu.state.process->memory.FreeMemory(mirror);
            memoryFreed = true;
        }
    }

    Texture::Texture(GPU &gpu, BackingType &&backing, texture::Dimensions dimensions, texture::Format format, vk::ImageLayout layout, vk::ImageTiling tiling, vk::ImageCreateFlags flags, vk::ImageUsageFlags usage, u32 levelCount, u32 layerCount, vk::SampleCountFlagBits sampleCount)
        : gpu(gpu),
          backing(std::move(backing)),
          dimensions(dimensions),
          format(format),
          layout(layout),
          tiling(tiling),
          flags(flags),
          usage(usage),
          levelCount(levelCount),
          layerCount(layerCount),
          sampleCount(sampleCount) {}

    texture::Format ConvertHostCompatibleFormat(texture::Format format, const TraitManager &traits) {
        auto bcnSupport{traits.bcnSupport};
        if (bcnSupport.all())
            return format;

        switch (format->vkFormat) {
            case vk::Format::eBc1RgbaUnormBlock:
                return bcnSupport[0] ? format : format::R8G8B8A8Unorm;
            case vk::Format::eBc1RgbaSrgbBlock:
                return bcnSupport[0] ? format : format::R8G8B8A8Srgb;

            case vk::Format::eBc2UnormBlock:
                return bcnSupport[1] ? format : format::R8G8B8A8Unorm;
            case vk::Format::eBc2SrgbBlock:
                return bcnSupport[1] ? format : format::R8G8B8A8Srgb;

            case vk::Format::eBc3UnormBlock:
                return bcnSupport[2] ? format : format::R8G8B8A8Unorm;
            case vk::Format::eBc3SrgbBlock:
                return bcnSupport[2] ? format : format::R8G8B8A8Srgb;

            case vk::Format::eBc4UnormBlock:
                return bcnSupport[3] ? format : format::R8Unorm;
            case vk::Format::eBc4SnormBlock:
                return bcnSupport[3] ? format : format::R8Snorm;

            case vk::Format::eBc5UnormBlock:
                return bcnSupport[4] ? format : format::R8G8Unorm;
            case vk::Format::eBc5SnormBlock:
                return bcnSupport[4] ? format : format::R8G8Snorm;

            case vk::Format::eBc6HUfloatBlock:
            case vk::Format::eBc6HSfloatBlock:
                return bcnSupport[5] ? format : format::R16G16B16A16Float; // This is a signed 16-bit FP format, we don't have an unsigned 16-bit FP format

            case vk::Format::eBc7UnormBlock:
                return bcnSupport[6] ? format : format::R8G8B8A8Unorm;
            case vk::Format::eBc7SrgbBlock:
                return bcnSupport[6] ? format : format::R8G8B8A8Srgb;

            default:
                return format;
        }
    }

    size_t CalculateLevelStride(const std::vector<texture::MipLevelLayout> &mipLayouts) {
        size_t surfaceSize{};
        for (const auto &level : mipLayouts)
            surfaceSize += level.linearSize;
        return surfaceSize;
    }

    size_t CalculateTargetLevelStride(const std::vector<texture::MipLevelLayout> &mipLayouts) {
        size_t surfaceSize{};
        for (const auto &level : mipLayouts)
            surfaceSize += level.targetLinearSize;
        return surfaceSize;
    }

    Texture::Texture(GPU &pGpu, GuestTexture pGuest)
        : gpu(pGpu),
          guest(std::move(pGuest)),
          dimensions(guest->dimensions),
          format(ConvertHostCompatibleFormat(guest->format, gpu.traits)),
          layout(vk::ImageLayout::eUndefined),
          tiling(vk::ImageTiling::eOptimal), // Force Optimal due to not adhering to host subresource layout during Linear synchronization
          layerCount(guest->layerCount),
          deswizzledLayerStride(static_cast<u32>(guest->format->GetSize(dimensions))),
          layerStride(format == guest->format ? deswizzledLayerStride : static_cast<u32>(format->GetSize(dimensions))),
          levelCount(guest->mipLevelCount),
          mipLayouts(
              texture::GetBlockLinearMipLayout(
                  guest->dimensions,
                  guest->format->blockHeight, guest->format->blockWidth, guest->format->bpb,
                  format->blockHeight, format->blockWidth, format->bpb,
                  guest->tileConfig.blockHeight, guest->tileConfig.blockDepth,
                  guest->mipLevelCount
              )
          ),
          deswizzledSurfaceSize(CalculateLevelStride(mipLayouts) * layerCount),
          surfaceSize(format == guest->format ? deswizzledSurfaceSize : (CalculateTargetLevelStride(mipLayouts) * layerCount)),
          sampleCount(vk::SampleCountFlagBits::e1),
          flags(gpu.traits.quirks.vkImageMutableFormatCostly ? vk::ImageCreateFlags{} : vk::ImageCreateFlagBits::eMutableFormat),
          usage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled) {
        if ((format->vkAspect & vk::ImageAspectFlagBits::eColor) && !format->IsCompressed())
            usage |= vk::ImageUsageFlagBits::eColorAttachment;
        if (format->vkAspect & (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil))
            usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;

        auto imageType{guest->GetImageType()};
        if (imageType == vk::ImageType::e2D && dimensions.width == dimensions.height && layerCount >= 6)
            flags |= vk::ImageCreateFlagBits::eCubeCompatible;
        else if (imageType == vk::ImageType::e3D)
            flags |= vk::ImageCreateFlagBits::e2DArrayCompatible;

        vk::ImageCreateInfo imageCreateInfo{
            .flags = flags,
            .imageType = imageType,
            .format = *format,
            .extent = dimensions,
            .mipLevels = levelCount,
            .arrayLayers = layerCount,
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
        SynchronizeGuest(true);
        if (trapHandle)
            gpu.state.nce->DeleteTrap(*trapHandle);
        if (alignedMirror.valid())
            munmap(alignedMirror.data(), alignedMirror.size());
    }

    void Texture::lock() {
        mutex.lock();
        accumulatedCpuLockCounter++;
    }

    bool Texture::LockWithTag(ContextTag pTag) {
        if (pTag && pTag == tag)
            return false;

        mutex.lock();
        tag = pTag;
        return true;
    }

    void Texture::unlock() {
        tag = ContextTag{};
        mutex.unlock();
    }

    bool Texture::try_lock() {
        if (mutex.try_lock()) {
            accumulatedCpuLockCounter++;
            return true;
        }

        return false;
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

        if (cycle) {
            cycle->Wait();
            cycle = nullptr;
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
                        .levelCount = levelCount,
                        .layerCount = layerCount,
                    },
                });
            })};
            lCycle->AttachObject(shared_from_this());
            cycle = lCycle;
        }
    }

    void Texture::MarkGpuDirty(UsageTracker &usageTracker) {
        for (auto mapping : guest->mappings)
            if (mapping.valid())
                usageTracker.dirtyIntervals.Insert(mapping);
    }

    void Texture::SynchronizeHost(bool gpuDirty) {
        if (!guest)
            return;

        // FIXME (TEXMAN): This should really be tracked on the texture usage side
        if (!*gpu.state.settings->freeGuestTextureMemory && !everUsedAsRt)
            gpuDirty = false;

        TRACE_EVENT("gpu", "Texture::SynchronizeHost");
        {
            std::scoped_lock lock{stateMutex};
            if (gpuDirty && dirtyState == DirtyState::Clean) {
                // If a texture is Clean then we can just transition it to being GPU dirty and retrap it
                dirtyState = DirtyState::GpuDirty;
                gpu.state.nce->TrapRegions(*trapHandle, false);
                FreeGuest();
                return;
            } else if (dirtyState != DirtyState::CpuDirty) {
                return; // If the texture has not been modified on the CPU, there is no need to synchronize it
            }

            dirtyState = gpuDirty ? DirtyState::GpuDirty : DirtyState::Clean;
            gpu.state.nce->TrapRegions(*trapHandle, !gpuDirty); // Trap any future CPU reads (optionally) + writes to this texture
        }

        // From this point on Clean -> CPU dirty state transitions can occur, GPU dirty -> * transitions will always require the full lock to be held and thus won't occur

        auto stagingBuffer{SynchronizeHostImpl()};
        if (stagingBuffer) {
            if (cycle)
                cycle->WaitSubmit();
            auto lCycle{gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                CopyFromStagingBuffer(commandBuffer, stagingBuffer);
            })};
            lCycle->AttachObjects(stagingBuffer, shared_from_this());
            lCycle->ChainCycle(cycle);
            cycle = lCycle;
        }

        {
            std::scoped_lock lock{stateMutex};

            if (dirtyState == DirtyState::GpuDirty)
                FreeGuest();
        }
    }

    void Texture::SynchronizeHostInline(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &pCycle, bool gpuDirty) {
        if (!guest)
            return;

        TRACE_EVENT("gpu", "Texture::SynchronizeHostInline");
        // FIXME (TEXMAN): This should really be tracked on the texture usage side
        if (!*gpu.state.settings->freeGuestTextureMemory && !everUsedAsRt)
            gpuDirty = false;

        {
            std::scoped_lock lock{stateMutex};
            if (gpuDirty && dirtyState == DirtyState::Clean) {
                dirtyState = DirtyState::GpuDirty;
                gpu.state.nce->TrapRegions(*trapHandle, false);
                FreeGuest();
                return;
            } else if (dirtyState != DirtyState::CpuDirty) {
                return;
            }

            dirtyState = gpuDirty ? DirtyState::GpuDirty : DirtyState::Clean;
            gpu.state.nce->TrapRegions(*trapHandle, !gpuDirty); // Trap any future CPU reads (optionally) + writes to this texture
        }

        auto stagingBuffer{SynchronizeHostImpl()};
        if (stagingBuffer) {
            CopyFromStagingBuffer(commandBuffer, stagingBuffer);
            pCycle->AttachObjects(stagingBuffer, shared_from_this());
            pCycle->ChainCycle(cycle);
            cycle = pCycle;
        }

        {
            std::scoped_lock lock{stateMutex};

            if (dirtyState == DirtyState::GpuDirty)
                FreeGuest();
        }
    }

    void Texture::SynchronizeGuest(bool cpuDirty, bool skipTrap) {
        if (!guest)
            return;

        TRACE_EVENT("gpu", "Texture::SynchronizeGuest");

        {
            std::scoped_lock lock{stateMutex};
            if (cpuDirty && dirtyState == DirtyState::Clean) {
                dirtyState = DirtyState::CpuDirty;
                if (!skipTrap)
                    gpu.state.nce->DeleteTrap(*trapHandle);
                return;
            } else if (dirtyState != DirtyState::GpuDirty) {
                return;
            }

            dirtyState = cpuDirty ? DirtyState::CpuDirty : DirtyState::Clean;
            memoryFreed = false;
        }

        if (layout == vk::ImageLayout::eUndefined || format != guest->format)
            // If the state of the host texture is undefined then so can the guest
            // If the texture has differing formats on the guest and host, we don't support converting back in that case as it may involve recompression of a decompressed texture
            return;

        WaitOnBacking();

        if (tiling == vk::ImageTiling::eOptimal || !std::holds_alternative<memory::Image>(backing)) {
            if (!downloadStagingBuffer)
                downloadStagingBuffer = gpu.memory.AllocateStagingBuffer(surfaceSize);

            WaitOnFence();
            auto lCycle{gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                CopyIntoStagingBuffer(commandBuffer, downloadStagingBuffer);
            })};
            lCycle->Wait(); // We block till the copy is complete

            CopyToGuest(downloadStagingBuffer->data());
        } else if (tiling == vk::ImageTiling::eLinear) {
            // We can optimize linear texture sync on a UMA by mapping the texture onto the CPU and copying directly from it rather than using a staging buffer
            WaitOnFence();
            CopyToGuest(std::get<memory::Image>(backing).data());
        } else {
            throw exception("Host -> Guest synchronization of images tiled as '{}' isn't implemented", vk::to_string(tiling));
        }

        if (!skipTrap)
            if (cpuDirty)
                gpu.state.nce->DeleteTrap(*trapHandle);
            else
                gpu.state.nce->TrapRegions(*trapHandle, true); // Trap any future CPU writes to this texture
    }

    std::shared_ptr<TextureView> Texture::GetView(vk::ImageViewType type, vk::ImageSubresourceRange range, texture::Format pFormat, vk::ComponentMapping mapping) {
        if (!pFormat || pFormat == guest->format)
            pFormat = format; // We want to use the texture's format if it isn't supplied or if the requested format matches the guest format then we want to use the host format just in case it is host incompatible and the host format differs from the guest format

        auto viewFormat{pFormat->vkFormat}, textureFormat{format->vkFormat};
        if (gpu.traits.quirks.vkImageMutableFormatCostly && viewFormat != textureFormat && (!gpu.traits.quirks.adrenoRelaxedFormatAliasing || !texture::IsAdrenoAliasCompatible(viewFormat, textureFormat)))
            Logger::Warn("Creating a view of a texture with a different format without mutable format: {} - {}", vk::to_string(viewFormat), vk::to_string(textureFormat));

        if ((pFormat->vkAspect & format->vkAspect) == vk::ImageAspectFlagBits{}) {
            pFormat = format; // If the requested format doesn't share any aspects then fallback to the texture's format in the hope it's more likely to function
            range.aspectMask = format->Aspect(mapping.r == vk::ComponentSwizzle::eR);
        }

        // Workaround to avoid aliasing when sampling from a BGRA texture with a RGBA view and a mapping to counteract that
        // TODO: drop this after new texture manager
        if (pFormat == format::R8G8B8A8Unorm && format == format::B8G8R8A8Unorm && mapping == vk::ComponentMapping{vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eA}) {
            pFormat = format;
            mapping = vk::ComponentMapping{};
        }

        return std::make_shared<TextureView>(shared_from_this(), type, range, pFormat, mapping);
    }

    void Texture::CopyFrom(std::shared_ptr<Texture> source, vk::Semaphore waitSemaphore, vk::Semaphore signalSemaphore, texture::Format srcFormat, const vk::ImageSubresourceRange &subresource) {
        if (cycle)
            cycle->WaitSubmit();
        if (source->cycle)
            source->cycle->WaitSubmit();

        WaitOnBacking();
        source->WaitOnBacking();
        WaitOnFence();

        if (source->layout == vk::ImageLayout::eUndefined)
            throw exception("Cannot copy from image with undefined layout");
        else if (source->dimensions != dimensions)
            throw exception("Cannot copy from image with different dimensions");

        TRACE_EVENT("gpu", "Texture::CopyFrom");

        auto submitFunc{[&](vk::Semaphore extraWaitSemaphore){
            boost::container::small_vector<vk::Semaphore, 2> waitSemaphores;
            if (waitSemaphore)
                waitSemaphores.push_back(waitSemaphore);

            if (extraWaitSemaphore)
                waitSemaphores.push_back(extraWaitSemaphore);

            return gpu.scheduler.Submit([&](vk::raii::CommandBuffer &commandBuffer) {
                auto sourceBacking{source->GetBacking()};
                if (source->layout != vk::ImageLayout::eTransferSrcOptimal) {
                    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, vk::ImageMemoryBarrier{
                        .image = sourceBacking,
                        .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
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
                    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, vk::ImageMemoryBarrier{
                        .image = destinationBacking,
                        .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
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
                for (; subresourceLayers.mipLevel < (subresource.levelCount == VK_REMAINING_MIP_LEVELS ? levelCount - subresource.baseMipLevel : subresource.levelCount); subresourceLayers.mipLevel++) {
                    if (srcFormat != format) {
                        commandBuffer.blitImage(sourceBacking, vk::ImageLayout::eTransferSrcOptimal, destinationBacking, vk::ImageLayout::eTransferDstOptimal, vk::ImageBlit{
                                .srcSubresource = subresourceLayers,
                                .srcOffsets = std::array<vk::Offset3D, 2>{
                                    vk::Offset3D{0, 0, 0},
                                    vk::Offset3D{static_cast<i32>(dimensions.width),
                                                 static_cast<i32>(dimensions.height),
                                                 static_cast<i32>(subresourceLayers.layerCount)}
                                },
                                .dstSubresource = subresourceLayers,
                                .dstOffsets = std::array<vk::Offset3D, 2>{
                                    vk::Offset3D{0, 0, 0},
                                    vk::Offset3D{static_cast<i32>(dimensions.width),
                                                 static_cast<i32>(dimensions.height),
                                                 static_cast<i32>(subresourceLayers.layerCount)}
                                }
                            }, vk::Filter::eLinear);
                    } else {
                        commandBuffer.copyImage(sourceBacking, vk::ImageLayout::eTransferSrcOptimal, destinationBacking, vk::ImageLayout::eTransferDstOptimal, vk::ImageCopy{
                            .srcSubresource = subresourceLayers,
                            .dstSubresource = subresourceLayers,
                            .extent = dimensions,
                        });
                    }
                }

                if (layout != vk::ImageLayout::eTransferDstOptimal)
                    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, vk::ImageMemoryBarrier{
                        .image = destinationBacking,
                        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                        .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                        .newLayout = layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .subresourceRange = subresource,
                        });

                if (source->layout != vk::ImageLayout::eTransferSrcOptimal)
                    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, vk::ImageMemoryBarrier{
                        .image = sourceBacking,
                        .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                        .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
                        .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                        .newLayout = source->layout,
                        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        .subresourceRange = subresource,
                        });
            }, waitSemaphores, span<vk::Semaphore>{signalSemaphore});
        }};

        auto newCycle{[&]{
            if (source->cycle)
                return source->cycle->RecordSemaphoreWaitUsage(std::move(submitFunc));
            else
                return submitFunc({});
        }()};
        newCycle->AttachObjects(std::move(source), shared_from_this());
        cycle = newCycle;
    }

    bool Texture::ValidateRenderPassUsage(u32 renderPassIndex, texture::RenderPassUsage renderPassUsage) {
        return lastRenderPassUsage == renderPassUsage || lastRenderPassIndex != renderPassIndex || lastRenderPassUsage == texture::RenderPassUsage::None;
    }

    void Texture::UpdateRenderPassUsage(u32 renderPassIndex, texture::RenderPassUsage renderPassUsage) {
        lastRenderPassUsage = renderPassUsage;
        lastRenderPassIndex = renderPassIndex;

        if (renderPassUsage == texture::RenderPassUsage::RenderTarget) {
            everUsedAsRt = true;
            pendingStageMask = vk::PipelineStageFlagBits::eVertexShader |
                vk::PipelineStageFlagBits::eTessellationControlShader |
                vk::PipelineStageFlagBits::eTessellationEvaluationShader |
                vk::PipelineStageFlagBits::eGeometryShader |
                vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eComputeShader;
            readStageMask = {};
        } else if (renderPassUsage == texture::RenderPassUsage::None) {
            pendingStageMask = {};
            readStageMask = {};
        }
    }

    texture::RenderPassUsage Texture::GetLastRenderPassUsage() {
        return lastRenderPassUsage;
    }

    vk::PipelineStageFlags Texture::GetReadStageMask() {
        return readStageMask;
    }

    void Texture::PopulateReadBarrier(vk::PipelineStageFlagBits dstStage, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask) {
        if (!guest)
            return;

        readStageMask |= dstStage;

        if (!(pendingStageMask & dstStage))
            return;

        if (format->vkAspect & (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil))
            srcStageMask |= vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
        else if (format->vkAspect & vk::ImageAspectFlagBits::eColor)
            srcStageMask |= vk::PipelineStageFlagBits::eColorAttachmentOutput;

        pendingStageMask &= ~dstStage;
        dstStageMask |= dstStage;
    }
}
