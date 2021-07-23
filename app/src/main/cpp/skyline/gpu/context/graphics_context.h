// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <soc/gm20b/engines/maxwell/types.h>

namespace skyline::gpu::context {
    namespace maxwell3d = soc::gm20b::engine::maxwell3d::type;

    /**
     * @brief Host-equivalent context for state of the Maxwell3D engine on the guest
     */
    class GraphicsContext {
      private:
        GPU &gpu;

        std::array<vk::Viewport, maxwell3d::ViewportCount> viewports;

        std::array<vk::Rect2D, maxwell3d::ViewportCount> scissors;
        constexpr static vk::Rect2D DefaultScissor{
            .extent = {
                .height = std::numeric_limits<i32>::max(),
                .width = std::numeric_limits<i32>::max(),
            }
        }; //!< A scissor which displays the entire viewport, utilized when the viewport scissor is disabled

      public:
        GraphicsContext(GPU &gpu) : gpu(gpu) {
            scissors.fill(DefaultScissor);
        }

        /* Viewport Transforms */

        /**
         * @url https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#vertexpostproc-viewport
         * @note Comments are written in the way of getting the same viewport transformations to be done on the host rather than deriving the host structure values from the guest submitted values, fundamentally the same thing but it is consistent with not assuming a certain guest API
         */
        void SetViewportX(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.x = scale - translate; // Counteract the addition of the half of the width (o_x) to the host translation
            viewport.width = scale * 2.0f; // Counteract the division of the width (p_x) by 2 for the host scale
        }

        void SetViewportY(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.y = scale - translate; // Counteract the addition of the half of the height (p_y/2 is center) to the host translation (o_y)
            viewport.height = scale * 2.0f; // Counteract the division of the height (p_y) by 2 for the host scale
        }

        void SetViewportZ(size_t index, float scale, float translate) {
            auto &viewport{viewports.at(index)};
            viewport.minDepth = translate; // minDepth (o_z) directly corresponds to the host translation
            viewport.maxDepth = scale + translate; // Counteract the subtraction of the maxDepth (p_z - o_z) by minDepth (o_z) for the host scale
        }

        /* Viewport Scissors */

        void SetScissor(size_t index, std::optional<maxwell3d::Scissor> scissor) {
            scissors.at(index) = scissor ? vk::Rect2D{
                .offset.x = scissor->horizontal.minimum,
                .extent.width = scissor->horizontal.maximum,
                .offset.y = scissor->vertical.minimum,
                .extent.height = scissor->horizontal.maximum,
            } : DefaultScissor;
        }

        void SetScissorHorizontal(size_t index, maxwell3d::Scissor::ScissorBounds bounds) {
            auto &scissor{scissors.at(index)};
            scissor.offset.x = bounds.minimum;
            scissor.extent.width = bounds.maximum;
        }

        void SetScissorVertical(size_t index, maxwell3d::Scissor::ScissorBounds bounds) {
            auto &scissor{scissors.at(index)};
            scissor.offset.y = bounds.minimum;
            scissor.extent.height = bounds.maximum;
        }
    };
}
