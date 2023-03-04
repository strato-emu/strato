// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <range/v3/view.hpp>
#include <range/v3/algorithm.hpp>
#include <common/spin_lock.h>
#include <common/lockable_shared_ptr.h>
#include <nce.h>
#include <gpu/tag_allocator.h>
#include <gpu/memory_manager.h>
#include <gpu/usage_tracker.h>

namespace skyline::gpu {
    namespace texture {
        enum class RenderPassUsage : u8 {
            None,
            Sampled,
            RenderTarget
        };

        struct Dimensions {
            u32 width;
            u32 height;
            u32 depth;

            constexpr Dimensions() : width(0), height(0), depth(0) {}

            constexpr Dimensions(u32 width) : width(width), height(1), depth(1) {}

            constexpr Dimensions(u32 width, u32 height) : width(width), height(height), depth(1) {}

            constexpr Dimensions(u32 width, u32 height, u32 depth) : width(width), height(height), depth(depth) {}

            constexpr Dimensions(vk::Extent2D extent) : Dimensions(extent.width, extent.height) {}

            constexpr Dimensions(vk::Extent3D extent) : Dimensions(extent.width, extent.height, extent.depth) {}

            auto operator<=>(const Dimensions &) const = default;

            constexpr operator vk::Extent2D() const {
                return vk::Extent2D{
                    .width = width,
                    .height = height,
                };
            }

            constexpr operator vk::Extent3D() const {
                return vk::Extent3D{
                    .width = width,
                    .height = height,
                    .depth = depth,
                };
            }

            /**
             * @return If the dimensions are valid and don't equate to zero
             */
            constexpr operator bool() const {
                return width && height && depth;
            }
        };

        /**
         * @note Blocks refers to the atomic unit of a compressed format (IE: The minimum amount of data that can be decompressed)
         */
        struct FormatBase {
            u8 bpb{}; //!< Bytes Per Block, this is used instead of bytes per pixel as that might not be a whole number for compressed formats
            vk::Format vkFormat{vk::Format::eUndefined};
            vk::ImageAspectFlags vkAspect{vk::ImageAspectFlagBits::eColor};
            u16 blockHeight{1}; //!< The height of a block in pixels
            u16 blockWidth{1}; //!< The width of a block in pixels
            vk::ComponentMapping swizzleMapping{
                .r = vk::ComponentSwizzle::eR,
                .g = vk::ComponentSwizzle::eG,
                .b = vk::ComponentSwizzle::eB,
                .a = vk::ComponentSwizzle::eA
            };
            bool stencilFirst{}; //!< If the stencil channel is the first channel in the format

            constexpr bool IsCompressed() const {
                return (blockHeight != 1) || (blockWidth != 1);
            }

            /**
             * @param width The width of the texture in pixels
             * @param height The height of the texture in pixels
             * @param depth The depth of the texture in layers
             * @return The size of the texture in bytes
             */
            constexpr size_t GetSize(u32 width, u32 height, u32 depth = 1) const {
                return util::DivideCeil<size_t>(width, size_t{blockWidth}) * util::DivideCeil<size_t>(height, size_t{blockHeight}) * bpb * depth;
            }

            constexpr size_t GetSize(Dimensions dimensions) const {
                return GetSize(dimensions.width, dimensions.height, dimensions.depth);
            }

            constexpr bool operator==(const FormatBase &format) const {
                return vkFormat == format.vkFormat;
            }

            constexpr bool operator!=(const FormatBase &format) const {
                return vkFormat != format.vkFormat;
            }

            constexpr operator vk::Format() const {
                return vkFormat;
            }

            /**
             * @return If this format is actually valid or not
             */
            constexpr operator bool() const {
                return bpb;
            }

            /**
             * @return If the supplied format is texel-layout compatible with the current format
             */
            constexpr bool IsCompatible(const FormatBase &other) const {
                return vkFormat == other.vkFormat
                    || (vkFormat == vk::Format::eD32Sfloat && other.vkFormat == vk::Format::eR32Sfloat)
                    || (componentCount(vkFormat) == componentCount(other.vkFormat) &&
                        ranges::all_of(ranges::views::iota(u8{0}, componentCount(vkFormat)), [this, other](auto i) {
                            return componentBits(vkFormat, i) == componentBits(other.vkFormat, i);
                        }) && (vkAspect & other.vkAspect) != vk::ImageAspectFlags{});
            }

            /**
             * @brief Determines the image aspect to use based off of the format and the first swizzle component
             */
            constexpr vk::ImageAspectFlags Aspect(bool first) const {
                if (vkAspect & vk::ImageAspectFlagBits::eDepth && vkAspect & vk::ImageAspectFlagBits::eStencil) {
                    if (first)
                        return stencilFirst ? vk::ImageAspectFlagBits::eStencil : vk::ImageAspectFlagBits::eDepth;
                    else
                        return stencilFirst ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eStencil;
                } else {
                    return vkAspect;
                }
            }
        };

        /**
         * @brief A wrapper around a pointer to underlying format metadata to prevent redundant copies
         * @note The equality operators **do not** compare equality for pointers but for the underlying formats while considering nullability
         */
        class Format {
          private:
            const FormatBase *base;

          public:
            constexpr Format(const FormatBase &base) : base(&base) {}

            constexpr Format() : base(nullptr) {}

            constexpr const FormatBase *operator->() const {
                return base;
            }

            constexpr const FormatBase &operator*() const {
                return *base;
            }

            constexpr bool operator==(const Format &format) const {
                return base && format.base ? (*base) == (*format.base) : base == format.base;
            }

            constexpr bool operator!=(const Format &format) const {
                return base && format.base ? (*base) != (*format.base) : base != format.base;
            }

            constexpr operator bool() const {
                return base;
            }
        };

        /**
         * @brief The layout of a texture in GPU memory
         * @note Refer to Chapter 20.1 of the Tegra X1 TRM for information
         */
        enum class TileMode {
            Linear, //!< All pixels are arranged linearly
            Pitch,  //!< All pixels are arranged linearly but rows aligned to the pitch
            Block,  //!< All pixels are arranged into blocks and swizzled in a Z-order curve to optimize for spacial locality
        };

        /**
         * @brief The parameters of the tiling mode, covered in Table 76 in the Tegra X1 TRM
         */
        struct TileConfig {
            TileMode mode;
            union {
                struct {
                    u8 blockHeight; //!< The height of the blocks in GOBs
                    u8 blockDepth;  //!< The depth of the blocks in GOBs
                };
                u32 pitch; //!< The pitch of the texture in bytes
            };

            constexpr bool operator==(const TileConfig &other) const {
                if (mode == other.mode) {
                    switch (mode) {
                        case TileMode::Linear:
                            return true;
                        case TileMode::Pitch:
                            return pitch == other.pitch;
                        case TileMode::Block:
                            return blockHeight == other.blockHeight && blockDepth == other.blockDepth;
                    }
                }

                return false;
            }
        };

        /**
         * @brief A description of a single mipmapped level of a block-linear surface
         */
        struct MipLevelLayout {
            Dimensions dimensions; //!< The dimensions of the mipmapped level, these are exact dimensions and not aligned to a GOB
            size_t linearSize; //!< The size of a linear image with this mipmapped level in bytes
            size_t targetLinearSize; //!< The size of a linear image with this mipmapped level in bytes and using the target format, this will only differ from linearSize if the target format is supplied
            size_t blockLinearSize; //!< The size of a blocklinear image with this mipmapped level in bytes
            size_t blockHeight, blockDepth; //!< The block height and block depth set for the level

            constexpr MipLevelLayout(Dimensions dimensions, size_t linearSize, size_t targetLinearSize, size_t blockLinearSize, size_t blockHeight, size_t blockDepth) : dimensions{dimensions}, linearSize{linearSize}, targetLinearSize{targetLinearSize}, blockLinearSize{blockLinearSize}, blockHeight{blockHeight}, blockDepth{blockDepth} {}
        };
    }

    class Texture;
    class PresentationEngine; //!< A forward declaration of PresentationEngine as we require it to be able to create a Texture object

    /**
     * @brief A descriptor for a texture present in guest memory, it can be used to create a corresponding Texture object for usage on the host
     */
    struct GuestTexture {
        using Mappings = boost::container::small_vector<span<u8>, 3>;

        Mappings mappings; //!< Spans to CPU memory for the underlying data backing this texture
        texture::Dimensions dimensions{};
        texture::Format format{};
        texture::TileConfig tileConfig{};
        vk::ImageViewType viewType{};
        u32 baseArrayLayer{};
        u32 layerCount{1};
        u32 layerStride{}; //!< An optional hint regarding the size of a single layer, it **should** be set to 0 when not available and should never be a non-0 value that doesn't reflect the correct layer stride
        u32 mipLevelCount{1}; //!< The total amount of mip levels in the parent image, if one exists
        u32 viewMipBase{}; //!< The minimum mip level of the view, this is the smallest mip level that can be accessed via this view
        u32 viewMipCount{1}; //!< The amount of mip levels starting from the base that can be accessed via this view
        vk::ComponentMapping swizzle{}; //!< Component swizzle derived from format requirements and the guest supplied swizzle
        vk::ImageAspectFlags aspect{};

        GuestTexture() {}

        GuestTexture(Mappings mappings, texture::Dimensions dimensions, texture::Format format, texture::TileConfig tileConfig, vk::ImageViewType viewType, u32 baseArrayLayer = 0, u32 layerCount = 1, u32 layerStride = 0, u32 mipLevelCount = 1, u32 viewMipBase = 0, u32 viewMipCount = 1)
            : mappings(mappings),
              dimensions(dimensions),
              format(format),
              tileConfig(tileConfig),
              viewType(viewType),
              baseArrayLayer(baseArrayLayer),
              layerCount(layerCount),
              layerStride(layerStride),
              mipLevelCount(mipLevelCount),
              viewMipBase(viewMipBase),
              viewMipCount(viewMipCount),
              aspect(format->vkAspect) {}

        GuestTexture(span<u8> mapping, texture::Dimensions dimensions, texture::Format format, texture::TileConfig tileConfig, vk::ImageViewType viewType, u32 baseArrayLayer = 0, u32 layerCount = 1, u32 layerStride = 0, u32 mipLevelCount = 1, u32 viewMipBase = 0, u32 viewMipCount = 1)
            : mappings(1, mapping),
              dimensions(dimensions),
              format(format),
              tileConfig(tileConfig),
              viewType(viewType),
              baseArrayLayer(baseArrayLayer),
              layerCount(layerCount),
              layerStride(layerStride),
              mipLevelCount(mipLevelCount),
              viewMipBase(viewMipBase),
              viewMipCount(viewMipCount),
              aspect(format->vkAspect) {}

        /**
         * @note This should be used over accessing the `layerStride` member directly when desiring the actual layer stride for calculations as it will automatically handle it not being filled in
         * @note Requires `dimensions`, `format` and `tileConfig` to be filled in
         * @return The size of a single layer with layout alignment in bytes
         */
        u32 GetLayerStride();

        /**
         * @brief Calculates the size of a single layer in bytes, unlike `GetLayerStride` the returned layer size is always calculated and may not be equal to the actual layer stride
         */
        u32 CalculateLayerSize() const;

        /**
         * @return The most appropriate backing image type for this texture
         */
        vk::ImageType GetImageType() const;

        u32 GetViewLayerCount() const;

        u32 GetViewDepth() const;

        size_t GetSize();

        bool MappingsValid() const;
    };

    class TextureManager;

    /**
     * @brief A view into a specific subresource of a Texture
     * @note The object **must** be locked prior to accessing any members as values will be mutated
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class TextureView : public std::enable_shared_from_this<TextureView> {
      private:
        vk::ImageView vkView{};

      public:
        LockableSharedPtr<Texture> texture;
        vk::ImageViewType type;
        texture::Format format;
        vk::ComponentMapping mapping;
        vk::ImageSubresourceRange range;

        /**
         * @param format A compatible format for the texture view (Defaults to the format of the backing texture)
         */
        TextureView(std::shared_ptr<Texture> texture, vk::ImageViewType type, vk::ImageSubresourceRange range, texture::Format format = {}, vk::ComponentMapping mapping = {});

        /**
         * @brief Acquires an exclusive lock on the backing texture for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock();

        /**
         * @brief Acquires an exclusive lock on the texture for the calling thread
         * @param tag A tag to associate with the lock, future invocations with the same tag prior to the unlock will acquire the lock without waiting (0 is not a valid tag value and will disable tag behavior)
         * @return If the lock was acquired by this call rather than having the same tag as the holder
         * @note All locks using the same tag **must** be from the same thread as it'll only have one corresponding unlock() call
         */
        bool LockWithTag(ContextTag tag);

        /**
         * @brief Relinquishes an existing lock on the backing texture by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock();

        /**
         * @brief Attempts to acquire an exclusive lock on the backing texture but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock();

        /**
         * @return A VkImageView that corresponds to the properties of this view
         * @note The texture **must** be locked prior to calling this
         */
        vk::ImageView GetView();

        bool operator==(const TextureView &rhs) {
            return texture == rhs.texture && type == rhs.type && format == rhs.format && mapping == rhs.mapping && range == rhs.range;
        }
    };

    /**
     * @brief A texture which is backed by host constructs while being synchronized with the underlying guest texture
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class Texture : public std::enable_shared_from_this<Texture> {
      private:
        GPU &gpu;
        RecursiveSpinLock mutex; //!< Synchronizes any mutations to the texture or its backing
        std::atomic<ContextTag> tag{}; //!< The tag associated with the last lock call
        std::condition_variable_any backingCondition; //!< Signalled when a valid backing has been swapped in
        using BackingType = std::variant<vk::Image, vk::raii::Image, memory::Image>;
        BackingType backing; //!< The Vulkan image that backs this texture, it is nullable

        span<u8> mirror{}; //!< A contiguous mirror of all the guest mappings to allow linear access on the CPU
        span<u8> alignedMirror{}; //!< The mirror mapping aligned to page size to reflect the full mapping
        std::optional<nce::NCE::TrapHandle> trapHandle{}; //!< The handle of the traps for the guest mappings
        enum class DirtyState {
            Clean, //!< The CPU mappings are in sync with the GPU texture
            CpuDirty, //!< The CPU mappings have been modified but the GPU texture is not up to date
            GpuDirty, //!< The GPU texture has been modified but the CPU mappings have not been updated
        } dirtyState{DirtyState::CpuDirty}; //!< The state of the CPU mappings with respect to the GPU texture
        bool memoryFreed{}; //!< If the guest backing memory has been freed
        std::recursive_mutex stateMutex; //!< Synchronizes access to the dirty state

        /**
         * @brief Storage for all metadata about a specific view into the buffer, used to prevent redundant view creation and duplication of VkBufferView(s)
         */
        struct TextureViewStorage {
            vk::ImageViewType type;
            texture::Format format;
            vk::ComponentMapping mapping;
            vk::ImageSubresourceRange range;
            vk::raii::ImageView vkView;

            TextureViewStorage(vk::ImageViewType type, texture::Format format, vk::ComponentMapping mapping, vk::ImageSubresourceRange range, vk::raii::ImageView &&vkView);
        };

        std::vector<TextureViewStorage> views;

        std::shared_ptr<memory::StagingBuffer> downloadStagingBuffer{};

        u32 lastRenderPassIndex{}; //!< The index of the last render pass that used this texture
        texture::RenderPassUsage lastRenderPassUsage{texture::RenderPassUsage::None}; //!< The type of usage in the last render pass
        bool everUsedAsRt{}; //!< If this texture has ever been used as a rendertarget
        vk::PipelineStageFlags pendingStageMask{}; //!< List of pipeline stages that are yet to be flushed for reads since the last time this texture was used an an RT
        vk::PipelineStageFlags readStageMask{}; //!< Set of pipeline stages that this texture has been read in since it was last used as an RT

        friend TextureManager;
        friend TextureView;

        /**
         * @brief Sets up mirror mappings for the guest mappings, this must be called after construction for the mirror to be valid
         */
        void SetupGuestMappings();

        /**
         * @brief An implementation function for guest -> host texture synchronization, it allocates and copies data into a staging buffer or directly into a linear host texture
         * @return If a staging buffer was required for the texture sync, it's returned filled with guest texture data and must be copied to the host texture by the callee
         */
        std::shared_ptr<memory::StagingBuffer> SynchronizeHostImpl();

        /**
         * @brief Records commands for copying data from a staging buffer to the texture's backing into the supplied command buffer
         */
        void CopyFromStagingBuffer(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<memory::StagingBuffer> &stagingBuffer);

        /**
         * @brief Records commands for copying data from the texture's backing to a staging buffer into the supplied command buffer
         * @note Any caller **must** ensure that the layout is not `eUndefined`
         */
        void CopyIntoStagingBuffer(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<memory::StagingBuffer> &stagingBuffer);

        /**
         * @brief Copies data from the supplied host buffer into the guest texture
         * @note The host buffer must be contain the entire image
         */
        void CopyToGuest(u8 *hostBuffer);

        /**
         * @brief Frees the guest side copy of the texture
         * @note `stateMutex` must be locked when calling this function
         */
        void FreeGuest();

        /**
         * @return A vector of all the buffer image copies that need to be done for every aspect of every level of every layer of the texture
         */
        boost::container::small_vector<vk::BufferImageCopy, 10> GetBufferImageCopies();

        static constexpr size_t FrequentlyLockedThreshold{2}; //!< Threshold for the number of times a texture can be locked (not from context locks, only normal) before it should be considered frequently locked
        size_t accumulatedCpuLockCounter{};

        static constexpr size_t SkipReadbackHackWaitCountThreshold{6}; //!< Threshold for the number of times a texture can be waited on before it should be considered for the readback hack
        static constexpr std::chrono::nanoseconds SkipReadbackHackWaitTimeThreshold{constant::NsInSecond / 4}; //!< Threshold for the amount of time a texture can be waited on before it should be considered for the readback hack, `SkipReadbackHackWaitCountThreshold` needs to be hit before this
        size_t accumulatedGuestWaitCounter{}; //!< Total number of times the texture has been waited on
        std::chrono::nanoseconds accumulatedGuestWaitTime{}; //!< Amount of time the texture has been waited on for since the `SkipReadbackHackWaitCountThreshold`th wait on it by the guest

      public:
        std::shared_ptr<FenceCycle> cycle; //!< A fence cycle for when any host operation mutating the texture has completed, it must be waited on prior to any mutations to the backing
        std::optional<GuestTexture> guest;
        texture::Dimensions dimensions;
        texture::Format format;
        vk::ImageLayout layout;
        vk::ImageTiling tiling;
        vk::ImageCreateFlags flags;
        vk::ImageUsageFlags usage;
        u32 layerCount; //!< The amount of array layers in the image
        size_t deswizzledLayerStride{}; //!< The stride of a single layer given linear tiling using the guest format, this does **not** consider mipmapping
        size_t layerStride{}; //!< The stride of a single layer given linear tiling, this does **not** consider mipmapping
        u32 levelCount;
        std::vector<texture::MipLevelLayout> mipLayouts; //!< The layout of each mip level in the guest texture
        size_t deswizzledSurfaceSize{}; //!< The size of the guest surface with linear tiling, calculated with the guest format which may differ from the host format
        size_t surfaceSize{}; //!< The size of the entire surface given linear tiling, this contains all mip levels and layers
        vk::SampleCountFlagBits sampleCount;
        bool replaced{};

        /**
         * @brief Creates a texture object wrapping the supplied backing with the supplied attributes
         * @param layout The initial layout of the texture, it **must** be eUndefined or ePreinitialized
         */
        Texture(GPU &gpu, BackingType &&backing, texture::Dimensions dimensions, texture::Format format, vk::ImageLayout layout, vk::ImageTiling tiling, vk::ImageCreateFlags flags, vk::ImageUsageFlags usage, u32 levelCount = 1, u32 layerCount = 1, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1);

        /**
         * @brief Creates a texture object wrapping the guest texture with a backing that can represent the guest texture data
         * @note The guest mappings will not be setup until SetupGuestMappings() is called
         */
        Texture(GPU &gpu, GuestTexture guest);

        ~Texture();

        /**
         * @note The handle returned is nullable and the appropriate precautions should be taken
         */
        constexpr vk::Image GetBacking() {
            return std::visit(VariantVisitor{
                [](vk::Image image) { return image; },
                [](const vk::raii::Image &image) { return *image; },
                [](const memory::Image &image) { return image.vkImage; },
            }, backing);
        }

        /**
         * @brief Acquires an exclusive lock on the texture for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock();

        /**
         * @brief Acquires an exclusive lock on the texture for the calling thread
         * @param tag A tag to associate with the lock, future invocations with the same tag prior to the unlock will acquire the lock without waiting (A default initialised tag will disable this behaviour)
         * @return If the lock was acquired by this call as opposed to the texture already being locked with the same tag
         * @note All locks using the same tag **must** be from the same thread as it'll only have one corresponding unlock() call
         */
        bool LockWithTag(ContextTag tag);

        /**
         * @brief Relinquishes an existing lock on the texture by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock();

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock();

        /**
         * @brief Waits on the texture backing to be a valid non-null Vulkan image
         * @return If the mutex could be unlocked during the function
         * @note The texture **must** be locked prior to calling this
         */
        bool WaitOnBacking();

        /**
         * @brief Waits on a fence cycle if it exists till it's signalled and resets it after
         * @note The texture **must** be locked prior to calling this
         */
        void WaitOnFence();

        /**
         * @note All memory residing in the current backing is not copied to the new backing, it must be handled externally
         * @note The texture **must** be locked prior to calling this
         */
        void SwapBacking(BackingType &&backing, vk::ImageLayout layout = vk::ImageLayout::eUndefined);

        /**
         * @brief Transitions the backing to the supplied layout, if the backing already is in this layout then this does nothing
         * @note The texture **must** be locked prior to calling this
         */
        void TransitionLayout(vk::ImageLayout layout);

        /**
         * @brief Marks the texture as being GPU dirty
         */
        void MarkGpuDirty(UsageTracker &usageTracker);

        /**
         * @brief Synchronizes the host texture with the guest after it has been modified
         * @param gpuDirty If true, the texture will be transitioned to being GpuDirty by this call
         * @note This function is not blocking and the synchronization will not be complete until the associated fence is signalled, it can be waited on with WaitOnFence()
         * @note The texture **must** be locked prior to calling this
         */
        void SynchronizeHost(bool gpuDirty = false);

        /**
         * @brief Same as SynchronizeHost but this records any commands into the supplied command buffer rather than creating one as necessary
         * @param gpuDirty If true, the texture will be transitioned to being GpuDirty by this call
         * @note It is more efficient to call SynchronizeHost than allocating a command buffer purely for this function as it may conditionally not record any commands
         * @note The texture **must** be locked prior to calling this
         */
        void SynchronizeHostInline(const vk::raii::CommandBuffer &commandBuffer, const std::shared_ptr<FenceCycle> &cycle, bool gpuDirty = false);

        /**
         * @brief Synchronizes the guest texture with the host texture after it has been modified
         * @param cpuDirty If true, the texture will be transitioned to being CpuDirty by this call
         * @param skipTrap If true, trapping/untrapping the guest mappings will be skipped and has to be handled by the caller
         * @note This function is blocking and waiting on the fence is not required
         * @note The texture **must** be locked prior to calling this
         */
        void SynchronizeGuest(bool cpuDirty = false, bool skipTrap = false);

        /**
         * @return A cached or newly created view into this texture with the supplied attributes
         */
        std::shared_ptr<TextureView> GetView(vk::ImageViewType type, vk::ImageSubresourceRange range, texture::Format format = {}, vk::ComponentMapping mapping = {});

        /**
         * @brief Copies the contents of the supplied source texture into the current texture
         */
        void CopyFrom(std::shared_ptr<Texture> source, vk::Semaphore waitSemaphore, vk::Semaphore signalSemaphore, texture::Format srcFormat, const vk::ImageSubresourceRange &subresource = vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        });

        /**
         * @return If the texture is frequently locked by threads using non-ContextLocks
         */
        bool FrequentlyLocked() {
            return accumulatedCpuLockCounter >= FrequentlyLockedThreshold;
        }

        /**
         * @brief Checks if the previous usage in the renderpass is compatible with the current one
         * @return If the new usage is compatible with the previous usage
         */
        bool ValidateRenderPassUsage(u32 renderPassIndex, texture::RenderPassUsage renderPassUsage);

        /**
         * @brief Updates renderpass usage tracking information
         */
        void UpdateRenderPassUsage(u32 renderPassIndex, texture::RenderPassUsage renderPassUsage);

        /**
         * @return The last usage of the texture
         */
        texture::RenderPassUsage GetLastRenderPassUsage();

        /**
         * @return The set of stages this texture has been read in since it was last used as an RT
         */
        vk::PipelineStageFlags GetReadStageMask();

        /**
         * @brief Populates the input src and dst stage masks with appropriate read barrier parameters for the current texture state
         */
        void PopulateReadBarrier(vk::PipelineStageFlagBits dstStage, vk::PipelineStageFlags &srcStageMask, vk::PipelineStageFlags &dstStageMask);
    };
}
