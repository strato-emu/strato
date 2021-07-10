// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <gpu/fence_cycle.h>

namespace skyline::gpu {
    namespace texture {
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

            constexpr vk::ImageType GetType() const {
                if (depth)
                    return vk::ImageType::e3D;
                else if (width)
                    return vk::ImageType::e2D;
                else
                    return vk::ImageType::e1D;
            }

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
        struct Format {
            u8 bpb{}; //!< Bytes Per Block, this is used instead of bytes per pixel as that might not be a whole number for compressed formats
            u16 blockHeight{}; //!< The height of a block in pixels
            u16 blockWidth{}; //!< The width of a block in pixels
            vk::Format vkFormat{vk::Format::eUndefined};

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
                return (((width / blockWidth) * (height / blockHeight)) * bpb) * depth;
            }

            constexpr size_t GetSize(Dimensions dimensions) const {
                return GetSize(dimensions.width, dimensions.height, dimensions.depth);
            }

            constexpr bool operator==(const Format &format) const {
                return vkFormat == format.vkFormat;
            }

            constexpr bool operator!=(const Format &format) const {
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
        union TileConfig {
            struct {
                u8 blockHeight; //!< The height of the blocks in GOBs
                u8 blockDepth;  //!< The depth of the blocks in GOBs
                u16 surfaceWidth;  //!< The width of a surface in samples
            };
            u32 pitch; //!< The pitch of the texture if it's pitch linear
        };

        enum class SwizzleChannel : u8 {
            Zero, //!< Write 0 to the channel
            One, //!< Write 1 to the channel
            Red, //!< Red color channel
            Green, //!< Green color channel
            Blue, //!< Blue color channel
            Alpha, //!< Alpha channel
        };

        struct Swizzle {
            SwizzleChannel red{SwizzleChannel::Red}; //!< Swizzle for the red channel
            SwizzleChannel green{SwizzleChannel::Green}; //!< Swizzle for the green channel
            SwizzleChannel blue{SwizzleChannel::Blue}; //!< Swizzle for the blue channel
            SwizzleChannel alpha{SwizzleChannel::Alpha}; //!< Swizzle for the alpha channel

            constexpr operator vk::ComponentMapping() {
                auto swizzleConvert{[](SwizzleChannel channel) {
                    switch (channel) {
                        case SwizzleChannel::Zero:
                            return vk::ComponentSwizzle::eZero;
                        case SwizzleChannel::One:
                            return vk::ComponentSwizzle::eOne;
                        case SwizzleChannel::Red:
                            return vk::ComponentSwizzle::eR;
                        case SwizzleChannel::Green:
                            return vk::ComponentSwizzle::eG;
                        case SwizzleChannel::Blue:
                            return vk::ComponentSwizzle::eB;
                        case SwizzleChannel::Alpha:
                            return vk::ComponentSwizzle::eA;
                    }
                }};

                return vk::ComponentMapping{
                    .r = swizzleConvert(red),
                    .g = swizzleConvert(green),
                    .b = swizzleConvert(blue),
                    .a = swizzleConvert(alpha),
                };
            }
        };
    }

    class Texture;
    class PresentationEngine; //!< A forward declaration of PresentationEngine as we require it to be able to create a Texture object

    /**
     * @brief A texture present in guest memory, it can be used to create a corresponding Texture object for usage on the host
     */
    class GuestTexture : public std::enable_shared_from_this<GuestTexture> {
      private:
        const DeviceState &state;

      public:
        u8 *pointer; //!< The address of the texture in guest memory
        std::weak_ptr<Texture> host; //!< A host texture (if any) that was created from this guest texture
        texture::Dimensions dimensions;
        texture::Format format;
        texture::TileMode tileMode;
        texture::TileConfig tileConfig;

        GuestTexture(const DeviceState &state, u8 *pointer, texture::Dimensions dimensions, const texture::Format &format, texture::TileMode tileMode = texture::TileMode::Linear, texture::TileConfig tileConfig = {});

        constexpr size_t Size() {
            return format.GetSize(dimensions);
        }

        /**
         * @brief Creates a corresponding host texture object for this guest texture
         * @param backing The Vulkan Image that is used as the backing on the host, its lifetime is not managed by the host texture object
         * @param dimensions The dimensions of the host texture (Defaults to the dimensions of the host texture)
         * @param format The format of the host texture (Defaults to the format of the guest texture)
         * @param tiling The tiling used by the image on host, this is the same as guest by default
         * @param layout The initial layout of the Vulkan Image, this is used for efficient layout management
         * @param swizzle The channel swizzle of the host texture (Defaults to no channel swizzling)
         * @return A shared pointer to the host texture object
         * @note There can only be one host texture for a corresponding guest texture
         * @note If any of the supplied parameters do not match up with the backing then it's undefined behavior
         */
        std::shared_ptr<Texture> InitializeTexture(vk::Image backing, texture::Dimensions dimensions = {}, const texture::Format &format = {}, std::optional<vk::ImageTiling> tiling = std::nullopt, vk::ImageLayout layout = vk::ImageLayout::eUndefined, texture::Swizzle swizzle = {});

        /**
         * @note As a RAII object is used here, the lifetime of the backing is handled by the host texture
         */
        std::shared_ptr<Texture> InitializeTexture(vk::raii::Image &&backing, std::optional<vk::ImageTiling> tiling = std::nullopt, vk::ImageLayout layout = vk::ImageLayout::eUndefined, const texture::Format &format = {}, texture::Dimensions dimensions = {}, texture::Swizzle swizzle = {});

        /**
         * @brief Similar to InitializeTexture but creation of the backing and allocation of memory for the backing is automatically performed by the function
         * @param usage Usage flags that will applied aside from VK_IMAGE_USAGE_TRANSFER_SRC_BIT/VK_IMAGE_USAGE_TRANSFER_DST_BIT which are mandatory
         */
        std::shared_ptr<Texture> CreateTexture(vk::ImageUsageFlags usage = {}, std::optional<vk::ImageTiling> tiling = std::nullopt, vk::ImageLayout initialLayout = vk::ImageLayout::eGeneral, const texture::Format &format = {}, texture::Dimensions dimensions = {}, texture::Swizzle swizzle = {});
    };

    /**
     * @brief A texture which is backed by host constructs while being synchronized with the underlying guest texture
     * @note This class conforms to the Lockable and BasicLockable C++ named requirements
     */
    class Texture : public std::enable_shared_from_this<Texture>, public FenceCycleDependency {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes any mutations to the texture or its backing
        std::condition_variable backingCondition; //!< Signalled when a valid backing has been swapped in
        using BackingType = std::variant<vk::Image, vk::raii::Image, memory::Image>;
        BackingType backing; //!< The Vulkan image that backs this texture, it is nullable
        std::shared_ptr<FenceCycle> cycle; //!< A fence cycle for when any host operation mutating the texture has completed, it must be waited on prior to any mutations to the backing
        vk::ImageLayout layout;

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

      public:
        std::shared_ptr<GuestTexture> guest; //!< The guest texture from which this was created, it's required for syncing
        texture::Dimensions dimensions;
        texture::Format format;
        vk::ImageTiling tiling;
        vk::ComponentMapping mapping;

        Texture(GPU &gpu, BackingType &&backing, std::shared_ptr<GuestTexture> guest, texture::Dimensions dimensions, const texture::Format &format, vk::ImageLayout layout, vk::ImageTiling tiling, vk::ComponentMapping mapping);

        Texture(GPU &gpu, BackingType &&backing, texture::Dimensions dimensions, const texture::Format &format, vk::ImageLayout layout, vk::ImageTiling tiling, vk::ComponentMapping mapping = {});

        /**
         * @brief Creates and allocates memory for the backing to creates a texture object wrapping it
         * @param usage Usage flags that will applied aside from VK_IMAGE_USAGE_TRANSFER_SRC_BIT/VK_IMAGE_USAGE_TRANSFER_DST_BIT which are mandatory
         */
        Texture(GPU &gpu, texture::Dimensions dimensions, const texture::Format &format, vk::ImageLayout initialLayout = vk::ImageLayout::eGeneral, vk::ImageUsageFlags usage = {}, vk::ImageTiling tiling = vk::ImageTiling::eOptimal, vk::ComponentMapping mapping = {});

        /**
         * @brief Acquires an exclusive lock on the texture for the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void lock() {
            mutex.lock();
        }

        /**
         * @brief Relinquishes an existing lock on the texture by the calling thread
         * @note Naming is in accordance to the BasicLockable named requirement
         */
        void unlock() {
            mutex.unlock();
        }

        /**
         * @brief Attempts to acquire an exclusive lock but returns immediately if it's captured by another thread
         * @note Naming is in accordance to the Lockable named requirement
         */
        bool try_lock() {
            return mutex.try_lock();
        }

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
         * @brief Convert this texture to the specified tiling mode
         * @param tileMode The tiling mode to convert it to
         * @param tileConfig The configuration for the tiling mode (Can be default argument for Linear)
         */
        void ConvertTileMode(texture::TileMode tileMode, texture::TileConfig tileConfig = {});

        /**
         * @brief Converts the texture dimensions to the specified ones (As long as they are within the GuestTexture's range)
         */
        void SetDimensions(texture::Dimensions dimensions);

        /**
         * @brief Converts the texture to have the specified format
         */
        void SetFormat(texture::Format format);

        /**
         * @brief Change the texture channel swizzle to the specified one
         */
        void SetSwizzle(texture::Swizzle swizzle);

        /**
         * @brief Synchronizes the host texture with the guest after it has been modified
         * @note The texture **must** be locked prior to calling this
         * @note The guest texture should not be null prior to calling this
         */
        void SynchronizeHost();

        /**
         * @brief Synchronizes the guest texture with the host texture after it has been modified
         * @note The texture **must** be locked prior to calling this
         * @note The guest texture should not be null prior to calling this
         */
        void SynchronizeGuest();

        /**
         * @brief Copies the contents of the supplied source texture into the current texture
         */
        void CopyFrom(std::shared_ptr<Texture> source);
    };
}
