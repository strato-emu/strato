// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "fence_cycle.h"

namespace skyline::gpu {
    /**
     * @brief The allocation and synchronized submission of command buffers to the host GPU is handled by this class
     */
    class CommandScheduler {
      private:
        /**
         * @brief A wrapper around a command buffer which tracks its state to avoid concurrent usage
         */
        struct CommandBufferSlot {
            std::atomic_flag active{true}; //!< If the command buffer is currently being recorded to
            const vk::raii::Device &device;
            vk::raii::CommandBuffer commandBuffer;
            vk::raii::Fence fence; //!< A fence used for tracking all submits of a buffer
            std::shared_ptr<FenceCycle> cycle; //!< The latest cycle on the fence, all waits must be performed through this

            CommandBufferSlot(vk::raii::Device &device, vk::CommandBuffer commandBuffer, vk::raii::CommandPool &pool);

            /**
             * @brief Attempts to allocate the buffer if it is free (Not being recorded/executing)
             * @return If the allocation was successful or not
             */
            static bool AllocateIfFree(CommandBufferSlot &slot);
        };

        /**
         * @brief An active command buffer occupies a slot and ensures that its status is updated correctly
         */
        class ActiveCommandBuffer {
          private:
            CommandBufferSlot &slot;

          public:
            constexpr ActiveCommandBuffer(CommandBufferSlot &slot) : slot(slot) {}

            ~ActiveCommandBuffer() {
                slot.active.clear();
            }

            vk::Fence GetFence() {
                return *slot.fence;
            }

            std::shared_ptr<FenceCycle> GetFenceCycle() {
                return slot.cycle;
            }

            vk::raii::CommandBuffer &operator*() {
                return slot.commandBuffer;
            }

            vk::raii::CommandBuffer *operator->() {
                return &slot.commandBuffer;
            }
        };

        GPU &gpu;
        std::mutex mutex; //!< Synchronizes mutations to the command pool due to allocations
        vk::raii::CommandPool vkCommandPool;
        std::list<CommandBufferSlot> commandBuffers;

        /**
         * @brief Allocates an existing or new primary command buffer from the pool
         */
        ActiveCommandBuffer AllocateCommandBuffer();

        /**
         * @brief Submits a single command buffer to the GPU queue with an optional fence
         */
        void SubmitCommandBuffer(const vk::raii::CommandBuffer &commandBuffer, vk::Fence fence = {});

      public:
        CommandScheduler(GPU &gpu);

        /**
         * @brief Submits a command buffer recorded with the supplied function synchronously
         */
        template<typename RecordFunction>
        std::shared_ptr<FenceCycle> Submit(RecordFunction recordFunction) {
            auto commandBuffer{AllocateCommandBuffer()};
            commandBuffer->begin(vk::CommandBufferBeginInfo{
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            });
            recordFunction(*commandBuffer);
            commandBuffer->end();
            SubmitCommandBuffer(*commandBuffer, commandBuffer.GetFence());
            return commandBuffer.GetFenceCycle();
        }
    };
}
