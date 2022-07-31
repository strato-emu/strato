// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <gpu/descriptor_allocator.h>
#include <gpu/cache/graphics_pipeline_cache.h>

namespace skyline::vfs {
    class FileSystem;
}

namespace skyline::gpu {
    class TextureView;
    class GPU;

    /**
     * @brief A base class that can be inherited by helper shaders that render to a single color rendertarget to simplify pipeline creation
     */
    class SimpleColourRTShader {
      protected:
        vk::raii::ShaderModule vertexShaderModule;
        vk::raii::ShaderModule fragmentShaderModule;

        std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages; //!< Shader stages for the vertex and fragment shader modules

        SimpleColourRTShader(GPU &gpu, std::shared_ptr<vfs::Backing> vertexShader, std::shared_ptr<vfs::Backing> fragmentShader);

        /**
         * @brief Returns a potentially cached pipeline built according to the supplied input state
         */
        cache::GraphicsPipelineCache::CompiledPipeline GetPipeline(GPU &gpu,
                                                                   TextureView *colorAttachment,
                                                                   span<const vk::DescriptorSetLayoutBinding> layoutBindings, span<const vk::PushConstantRange> pushConstantRanges);
    };

    /**
     * @brief Simple helper shader for blitting a texture to a rendertarget with subpixel-precision
     */
    class BlitHelperShader : SimpleColourRTShader {
      private:
        vk::raii::Sampler bilinearSampler;
        vk::raii::Sampler nearestSampler;

      public:
            BlitHelperShader(GPU &gpu, std::shared_ptr<vfs::FileSystem> shaderFileSystem);

            /**
             * @brief Floating point equivalent to vk::Rect2D to allow for subpixel-precison blits
             */
            struct BlitRect {
                float width;
                float height;
                float x;
                float y;
            };

            /**
             * @brief Records a sequenced GPU blit operation
             * @param srcRect A subrect of the source input texture that will be blitted from
             * @param dstRect A subrect of the destination input texture that the source subrect will be blitted into
             * @param dstSrcScaleFactorX Scale factor in the X direction from the destination image to the source image
             * @param dstSrcScaleFactorY ^ but Y
             * @param bilinearFilter Type of filter to use for sampling the source texture, false will use nearest-neighbour and true will use bilinear filtering
             * @param recordCb Callback used to record the blit commands for sequenced execution on the GPU
             */
            void Blit(GPU &gpu, BlitRect srcRect, BlitRect dstRect,
                      vk::Extent2D srcImageDimensions, vk::Extent2D dstImageDimensions,
                      float dstSrcScaleFactorX, float dstSrcScaleFactorY,
                      bool bilinearFilter,
                      TextureView *srcImageView, TextureView *dstImageView,
                      std::function<void(std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &, vk::RenderPass, u32)> &&)> &&recordCb);
    };

    /**
     * @brief Holds all helper shaders to avoid redundantly recreating them on each usage
     */
    struct HelperShaders {
        BlitHelperShader blitHelperShader;

        HelperShaders(GPU &gpu, std::shared_ptr<vfs::FileSystem> shaderFileSystem);
    };


}
