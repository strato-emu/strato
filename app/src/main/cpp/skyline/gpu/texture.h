// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <vulkan/vulkan.hpp>

namespace skyline {
    namespace service::hosbinder {
        class IHOSBinderDriver;
    }
    namespace gpu {
        namespace texture {
            /*
             * @brief This is used to hold the dimensions of a surface
             */
            struct Dimensions {
                u32 width; //!< The width of the surface
                u32 height; //!< The height of the surface
                u32 depth; //!< The depth of the surface

                constexpr Dimensions() : width(0), height(0), depth(0) {}

                constexpr Dimensions(u32 width, u32 height) : width(width), height(height), depth(1) {}

                constexpr Dimensions(u32 width, u32 height, u32 depth) : width(width), height(height), depth(depth) {}

                /**
                 * @return If the specified dimension is equal to this one
                 */
                constexpr inline bool operator==(const Dimensions &dimensions) {
                    return (width == dimensions.width) && (height == dimensions.height) && (depth == dimensions.depth);
                }

                /**
                 * @return If the specified dimension is not equal to this one
                 */
                constexpr inline bool operator!=(const Dimensions &dimensions) {
                    return (width != dimensions.width) || (height != dimensions.height) || (depth != dimensions.depth);
                }
            };

            /**
             * @brief This is used to hold the attributes of a texture format
             */
            struct Format {
                u8 bpb; //!< Bytes Per Block, this is to accommodate compressed formats
                u16 blockHeight; //!< The height of a single block
                u16 blockWidth; //!< The width of a single block
                vk::Format vkFormat; //!< The underlying Vulkan type of the format

                /**
                 * @return If this is a compressed texture format or not
                 */
                inline constexpr bool IsCompressed() {
                    return (blockHeight != 1) || (blockWidth != 1);
                }

                /**
                 * @param width The width of the texture in pixels
                 * @param height The height of the texture in pixels
                 * @param depth The depth of the texture in layers
                 * @return The size of the texture in bytes
                 */
                inline constexpr size_t GetSize(u32 width, u32 height, u32 depth = 1) {
                    return (((width / blockWidth) * (height / blockHeight)) * bpb) * depth;
                }

                /**
                 * @param dimensions The dimensions of a texture
                 * @return The size of the texture in bytes
                 */
                inline constexpr size_t GetSize(Dimensions dimensions) {
                    return GetSize(dimensions.width, dimensions.height, dimensions.depth);
                }

                /**
                 * @return If the specified format is equal to this one
                 */
                inline constexpr bool operator==(const Format &format) {
                    return vkFormat == format.vkFormat;
                }

                /**
                 * @return If the specified format is not equal to this one
                 */
                inline constexpr bool operator!=(const Format &format) {
                    return vkFormat != format.vkFormat;
                }

                /**
                 * @return If this format is actually valid or not
                 */
                inline constexpr operator bool() {
                    return bpb;
                }
            };

            namespace format {
                constexpr Format RGBA8888Unorm{sizeof(u8) * 4, 1, 1, vk::Format::eR8G8B8A8Unorm}; //!< 8-bits per channel 4-channel pixels
                constexpr Format RGB565Unorm{sizeof(u8) * 2, 1, 1, vk::Format::eR5G6B5UnormPack16}; //!< Red channel: 5-bit, Green channel: 6-bit, Blue channel: 5-bit
            }

            /**
             * @brief This describes the linearity of a texture. Refer to Chapter 20.1 of the Tegra X1 TRM for information.
             */
            enum class TileMode {
                Linear, //!< This is a purely linear texture
                Pitch,  //!< This is a pitch-linear texture
                Block,  //!< This is a 16Bx2 block-linear texture
            };

            /**
             * @brief This holds the parameters of the tiling mode, covered in Table 76 in the Tegra X1 TRM
             */
            union TileConfig {
                struct {
                    u8 blockHeight; //!< The height of the blocks in GOBs
                    u8 blockDepth;  //!< The depth of the blocks in GOBs
                    u16 surfaceWidth;  //!< The width of a surface in samples
                };
                u32 pitch; //!< The pitch of the texture if it's pitch linear
            };

            /**
             * @brief This enumerates all of the channel swizzle options
             */
            enum class SwizzleChannel {
                Zero, //!< Write 0 to the channel
                One, //!< Write 1 to the channel
                Red, //!< Red color channel
                Green, //!< Green color channel
                Blue, //!< Blue color channel
                Alpha, //!< Alpha channel
            };

            /**
             * @brief This holds all of the texture swizzles on each color channel
             */
            struct Swizzle {
                SwizzleChannel red{SwizzleChannel::Red}; //!< Swizzle for the red channel
                SwizzleChannel green{SwizzleChannel::Green}; //!< Swizzle for the green channel
                SwizzleChannel blue{SwizzleChannel::Blue}; //!< Swizzle for the blue channel
                SwizzleChannel alpha{SwizzleChannel::Alpha}; //!< Swizzle for the alpha channel
            };
        }

        class Texture;
        class PresentationTexture;

        /**
         * @brief This class is used to hold metadata about a guest texture and can be used to create a host Texture object
         */
        class GuestTexture : public std::enable_shared_from_this<GuestTexture> {
          private:
            const DeviceState &state; //!< The state of the device

          public:
            u64 address; //!< The address of the texture in guest memory
            std::shared_ptr<Texture> host; //!< The corresponding host texture object
            texture::Dimensions dimensions; //!< The dimensions of the texture
            texture::Format format; //!< The format of the texture
            texture::TileMode tileMode; //!< The tiling mode of the texture
            texture::TileConfig tileConfig; //!< The tiling configuration of the texture

            GuestTexture(const DeviceState &state, u64 address, texture::Dimensions dimensions, texture::Format format, texture::TileMode tileMode = texture::TileMode::Linear, texture::TileConfig tileConfig = {});

            inline constexpr size_t Size() {
                return format.GetSize(dimensions);
            }

            /**
             * @brief This creates a corresponding host texture object for this guest texture
             * @param format The format of the host texture (Defaults to the format of the guest texture)
             * @param dimensions The dimensions of the host texture (Defaults to the dimensions of the host texture)
             * @param swizzle The channel swizzle of the host texture (Defaults to no channel swizzling)
             * @return A shared pointer to the host texture object
             * @note There can only be one host texture for a corresponding guest texture
             */
            std::shared_ptr<Texture> InitializeTexture(std::optional<texture::Format> format = std::nullopt, std::optional<texture::Dimensions> dimensions = std::nullopt, texture::Swizzle swizzle = {}) {
                if (host)
                    throw exception("Trying to create multiple Texture objects from a single GuestTexture");
                host = std::make_shared<Texture>(state, shared_from_this(), dimensions ? *dimensions : this->dimensions, format ? *format : this->format, swizzle);
                return host;
            }

          protected:
            std::shared_ptr<PresentationTexture> InitializePresentationTexture() {
                if (host)
                    throw exception("Trying to create multiple PresentationTexture objects from a single GuestTexture");
                auto presentation = std::make_shared<PresentationTexture>(state, shared_from_this(), dimensions, format);
                host = std::static_pointer_cast<Texture>(presentation);
                return presentation;
            }

            friend service::hosbinder::IHOSBinderDriver;
        };

        /**
         * @brief This class is used to store a texture which is backed by host objects
         */
        class Texture {
          private:
            const DeviceState &state; //!< The state of the device

          public:
            std::vector<u8> backing; //!< The object that holds a host copy of the guest texture (Will be replaced with a vk::Image)
            std::shared_ptr<GuestTexture> guest; //!< The corresponding guest texture object
            texture::Dimensions dimensions; //!< The dimensions of the texture
            texture::Format format; //!< The format of the host texture
            texture::Swizzle swizzle; //!< The swizzle of the host texture

          public:
            Texture(const DeviceState &state, std::shared_ptr<GuestTexture> guest, texture::Dimensions dimensions, texture::Format format, texture::Swizzle swizzle);

          public:
            /**
             * @brief This convert this texture to the specified tiling mode
             * @param tileMode The tiling mode to convert it to
             * @param tileConfig The configuration for the tiling mode (Can be default argument for Linear)
             */
            void ConvertTileMode(texture::TileMode tileMode, texture::TileConfig tileConfig = {});

            /**
             * @brief This sets the texture dimensions to the specified ones (As long as they are within the GuestTexture's range)
             * @param dimensions The dimensions to adjust the texture to
             */
            void SetDimensions(texture::Dimensions dimensions);

            /**
             * @brief This sets the format to the specified one
             * @param format The format to change the texture to
             */
            void SetFormat(texture::Format format);

            /**
             * @brief This sets the channel swizzle to the specified one
             * @param swizzle The channel swizzle to the change the texture to
             */
            void SetSwizzle(texture::Swizzle swizzle);

            /**
             * @brief This synchronizes the host texture with the guest after it has been modified
             */
            void SynchronizeHost();

            /**
             * @brief This synchronizes the guest texture with the host texture after it has been modified
             */
            void SynchronizeGuest();
        };

        /**
         * @brief This class is used to hold a texture object alongside a release callback used for display presentation
         */
        class PresentationTexture : public Texture {
          public:
            std::function<void()> releaseCallback; //!< The release callback after this texture has been displayed

            PresentationTexture(const DeviceState &state, const std::shared_ptr<GuestTexture> &guest, const texture::Dimensions &dimensions, const texture::Format &format, const std::function<void()> &releaseCallback = {});

            /**
             * @return The corresponding Android surface format for the current texture format
             */
            i32 GetAndroidFormat();
        };
    }
}
