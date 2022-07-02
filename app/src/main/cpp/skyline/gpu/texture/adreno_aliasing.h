// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/static_vector.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <common.h>

namespace skyline::gpu::texture {
    /**
     * @return If a particular format is compatible to alias views of without VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT on Adreno GPUs
     */
    bool IsAdrenoAliasCompatible(vk::Format lhs, vk::Format rhs) {
        if (lhs <= vk::Format::eUndefined || lhs >= vk::Format::eB10G11R11UfloatPack32 ||
            rhs <= vk::Format::eUndefined || rhs >= vk::Format::eB10G11R11UfloatPack32)
            return false; // Any complex (compressed/multi-planar/etc) formats cannot be properly aliased

        constexpr size_t MaxComponentCount{4};
        using ComponentContainer = boost::container::static_vector<int, MaxComponentCount>;
        auto GetComponents{[](vk::Format format) -> ComponentContainer {
            #define FMT(name, ...) case vk::Format::e ## name: return ComponentContainer({__VA_ARGS__})

            switch (format) {
                FMT(R4G4UnormPack8, 4, 4);
                FMT(R4G4B4A4UnormPack16, 4, 4, 4, 4);
                FMT(B4G4R4A4UnormPack16, 4, 4, 4, 4);
                FMT(R5G6B5UnormPack16, 5, 6, 5);
                FMT(B5G6R5UnormPack16, 5, 6, 5);
                FMT(R5G5B5A1UnormPack16, 1, 5, 5, 5);
                FMT(B5G5R5A1UnormPack16, 1, 5, 5, 5);
                FMT(A1R5G5B5UnormPack16, 5, 5, 5, 1);
                FMT(R8Unorm, 8);
                FMT(R8Snorm, 8);
                FMT(R8Uscaled, 8);
                FMT(R8Sscaled, 8);
                FMT(R8Uint, 8);
                FMT(R8Sint, 8);
                FMT(R8Srgb, 8);
                FMT(R8G8Unorm, 8, 8);
                FMT(R8G8Snorm, 8, 8);
                FMT(R8G8Uscaled, 8, 8);
                FMT(R8G8Sscaled, 8, 8);
                FMT(R8G8Uint, 8, 8);
                FMT(R8G8Sint, 8, 8);
                FMT(R8G8Srgb, 8, 8);
                FMT(R8G8B8Unorm, 8, 8, 8);
                FMT(R8G8B8Snorm, 8, 8, 8);
                FMT(R8G8B8Uscaled, 8, 8, 8);
                FMT(R8G8B8Sscaled, 8, 8, 8);
                FMT(R8G8B8Uint, 8, 8, 8);
                FMT(R8G8B8Sint, 8, 8, 8);
                FMT(R8G8B8Srgb, 8, 8, 8);
                FMT(B8G8R8Unorm, 8, 8, 8);
                FMT(B8G8R8Snorm, 8, 8, 8);
                FMT(B8G8R8Uscaled, 8, 8, 8);
                FMT(B8G8R8Sscaled, 8, 8, 8);
                FMT(B8G8R8Uint, 8, 8, 8);
                FMT(B8G8R8Sint, 8, 8, 8);
                FMT(B8G8R8Srgb, 8, 8, 8);
                FMT(R8G8B8A8Unorm, 8, 8, 8, 8);
                FMT(R8G8B8A8Snorm, 8, 8, 8, 8);
                FMT(R8G8B8A8Uscaled, 8, 8, 8, 8);
                FMT(R8G8B8A8Sscaled, 8, 8, 8, 8);
                FMT(R8G8B8A8Uint, 8, 8, 8, 8);
                FMT(R8G8B8A8Sint, 8, 8, 8, 8);
                FMT(R8G8B8A8Srgb, 8, 8, 8, 8);
                FMT(B8G8R8A8Unorm, 8, 8, 8, 8);
                FMT(B8G8R8A8Snorm, 8, 8, 8, 8);
                FMT(B8G8R8A8Uscaled, 8, 8, 8, 8);
                FMT(B8G8R8A8Sscaled, 8, 8, 8, 8);
                FMT(B8G8R8A8Uint, 8, 8, 8, 8);
                FMT(B8G8R8A8Sint, 8, 8, 8, 8);
                FMT(B8G8R8A8Srgb, 8, 8, 8, 8);
                FMT(A8B8G8R8UnormPack32, 8, 8, 8, 8);
                FMT(A8B8G8R8SnormPack32, 8, 8, 8, 8);
                FMT(A8B8G8R8UscaledPack32, 8, 8, 8, 8);
                FMT(A8B8G8R8SscaledPack32, 8, 8, 8, 8);
                FMT(A8B8G8R8UintPack32, 8, 8, 8, 8);
                FMT(A8B8G8R8SintPack32, 8, 8, 8, 8);
                FMT(A8B8G8R8SrgbPack32, 8, 8, 8, 8);
                FMT(A2R10G10B10UnormPack32, 10, 10, 10, 2);
                FMT(A2R10G10B10SnormPack32, 10, 10, 10, 2);
                FMT(A2R10G10B10UscaledPack32, 10, 10, 10, 2);
                FMT(A2R10G10B10SscaledPack32, 10, 10, 10, 2);
                FMT(A2R10G10B10UintPack32, 10, 10, 10, 2);
                FMT(A2R10G10B10SintPack32, 10, 10, 10, 2);
                FMT(A2B10G10R10UnormPack32, 10, 10, 10, 2);
                FMT(A2B10G10R10SnormPack32, 10, 10, 10, 2);
                FMT(A2B10G10R10UscaledPack32, 10, 10, 10, 2);
                FMT(A2B10G10R10SscaledPack32, 10, 10, 10, 2);
                FMT(A2B10G10R10UintPack32, 10, 10, 10, 2);
                FMT(A2B10G10R10SintPack32, 10, 10, 10, 2);
                FMT(R16Unorm, 16);
                FMT(R16Snorm, 16);
                FMT(R16Uscaled, 16);
                FMT(R16Sscaled, 16);
                FMT(R16Uint, 16);
                FMT(R16Sint, 16);
                FMT(R16Sfloat, 16);
                FMT(R16G16Unorm, 16, 16);
                FMT(R16G16Snorm, 16, 16);
                FMT(R16G16Uscaled, 16, 16);
                FMT(R16G16Sscaled, 16, 16);
                FMT(R16G16Uint, 16, 16);
                FMT(R16G16Sint, 16, 16);
                FMT(R16G16Sfloat, 16, 16);
                FMT(R16G16B16Unorm, 16, 16, 16);
                FMT(R16G16B16Snorm, 16, 16, 16);
                FMT(R16G16B16Uscaled, 16, 16, 16);
                FMT(R16G16B16Sscaled, 16, 16, 16);
                FMT(R16G16B16Uint, 16, 16, 16);
                FMT(R16G16B16Sint, 16, 16, 16);
                FMT(R16G16B16Sfloat, 16, 16, 16);
                FMT(R16G16B16A16Unorm, 16, 16, 16, 16);
                FMT(R16G16B16A16Snorm, 16, 16, 16, 16);
                FMT(R16G16B16A16Uscaled, 16, 16, 16, 16);
                FMT(R16G16B16A16Sscaled, 16, 16, 16, 16);
                FMT(R16G16B16A16Uint, 16, 16, 16, 16);
                FMT(R16G16B16A16Sint, 16, 16, 16, 16);
                FMT(R16G16B16A16Sfloat, 16, 16, 16, 16);
                FMT(R32Uint, 32);
                FMT(R32Sint, 32);
                FMT(R32Sfloat, 32);
                FMT(R32G32Uint, 32, 32);
                FMT(R32G32Sint, 32, 32);
                FMT(R32G32Sfloat, 32, 32);
                FMT(R32G32B32Uint, 32, 32, 32);
                FMT(R32G32B32Sint, 32, 32, 32);
                FMT(R32G32B32Sfloat, 32, 32, 32);
                FMT(R32G32B32A32Uint, 32, 32, 32, 32);
                FMT(R32G32B32A32Sint, 32, 32, 32, 32);
                FMT(R32G32B32A32Sfloat, 32, 32, 32, 32);
                FMT(R64Uint, 64);
                FMT(R64Sint, 64);
                FMT(R64Sfloat, 64);
                FMT(R64G64Uint, 64, 64);
                FMT(R64G64Sint, 64, 64);
                FMT(R64G64Sfloat, 64, 64);
                FMT(R64G64B64Uint, 64, 64, 64);
                FMT(R64G64B64Sint, 64, 64, 64);
                FMT(R64G64B64Sfloat, 64, 64, 64);
                FMT(R64G64B64A64Uint, 64, 64, 64, 64);
                FMT(R64G64B64A64Sint, 64, 64, 64, 64);
                FMT(R64G64B64A64Sfloat, 64, 64, 64, 64);
                FMT(B10G11R11UfloatPack32, 11, 11, 10);

                default:
                    return {};
            }

            #undef FMT
        }};

        // We need to ensure that both formats have corresponding component bits in the same order
        auto lhsComponents{GetComponents(lhs)}, rhsComponents{GetComponents(rhs)};
        if (lhsComponents.empty() || rhsComponents.empty())
            return false;
        return lhsComponents == rhsComponents;
    }
}
