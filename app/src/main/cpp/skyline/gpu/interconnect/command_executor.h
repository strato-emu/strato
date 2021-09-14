// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <boost/container/stable_vector.hpp>
#include "command_nodes.h"

namespace skyline::gpu::interconnect {
    /**
     * @brief Assembles a Vulkan command stream with various nodes and manages execution of the produced graph
     * @note This class is **NOT** thread-safe and should not be utilized by multiple threads concurrently
     */
    class CommandExecutor {
      private:
        GPU &gpu;
        boost::container::stable_vector<node::NodeVariant> nodes;
        node::RenderpassNode *renderpass{};

      public:
        CommandExecutor(const DeviceState &state) : gpu(*state.gpu) {}

        /**
         * @brief Adds a command that needs to be executed inside a subpass configured with certain attachments
         * @note Any texture supplied to this **must** be locked by the calling thread, it should also undergo no persistent layout transitions till execution
         */
        void AddSubpass(const std::function<void(vk::raii::CommandBuffer &, const std::shared_ptr<FenceCycle> &, GPU &)> &function, vk::Rect2D renderArea, std::vector<TextureView> inputAttachments = {}, std::vector<TextureView> colorAttachments = {}, std::optional<TextureView> depthStencilAttachment = {});

        void Execute();
    };
}
