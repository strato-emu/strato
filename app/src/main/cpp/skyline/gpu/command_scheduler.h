// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/thread_local.h>
#include <common/circular_queue.h>
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
            vk::raii::Semaphore semaphore; //!< A semaphore used for tracking work status on the GPU
            std::shared_ptr<FenceCycle> cycle; //!< The latest cycle on the fence, all waits must be performed through this

            CommandBufferSlot(vk::raii::Device &device, vk::CommandBuffer commandBuffer, vk::raii::CommandPool &pool);
        };

        const DeviceState &state;
        GPU &gpu;

        /**
         * @brief A command pool designed to be thread-local to respect external synchronization for all command buffers and the associated pool
         * @note If we utilized a single global pool there would need to be a mutex around command buffer recording which would incur significant costs
         */
        struct CommandPool {
            vk::raii::CommandPool vkCommandPool;
            std::list<CommandBufferSlot> buffers;

            template<typename... Args>
            constexpr CommandPool(Args &&... args) : vkCommandPool(std::forward<Args>(args)...) {}
        };
        ThreadLocal<CommandPool> pool;

        std::thread waiterThread; //!< A thread that waits on and signals FenceCycle(s) then clears any associated resources
        static constexpr size_t FenceCycleWaitCount{256}; //!< The amount of fence cycles the cycle queue can hold
        CircularQueue<std::shared_ptr<FenceCycle>> cycleQueue{FenceCycleWaitCount}; //!< A circular queue containing all the active cycles that can be waited on

        void WaiterThread();

      public:
        /**
         * @brief An active command buffer occupies a slot and ensures that its status is updated correctly
         */
        class ActiveCommandBuffer {
          private:
            CommandBufferSlot *slot;

          public:
            constexpr ActiveCommandBuffer(CommandBufferSlot &slot) : slot{&slot} {}

            constexpr ActiveCommandBuffer &operator=(ActiveCommandBuffer &&other) {
                if (slot)
                    slot->active.clear(std::memory_order_release);
                slot = other.slot;
                other.slot = nullptr;
                return *this;
            }

            ~ActiveCommandBuffer() {
                if (slot)
                    slot->active.clear(std::memory_order_release);
            }

            vk::Fence GetFence() {
                return *slot->fence;
            }

            std::shared_ptr<FenceCycle> GetFenceCycle() {
                return slot->cycle;
            }

            vk::raii::CommandBuffer &operator*() {
                return slot->commandBuffer;
            }

            vk::raii::CommandBuffer *operator->() {
                return &slot->commandBuffer;
            }

            /**
             * @brief Resets the state of the command buffer with a new FenceCycle
             * @note This should be used when a single allocated command buffer is used for all submissions from a component
             */
            std::shared_ptr<FenceCycle> Reset() {
                slot->cycle->Wait();
                slot->cycle = std::make_shared<FenceCycle>(*slot->cycle);
                slot->commandBuffer.reset();
                return slot->cycle;
            }
        };

        CommandScheduler(const DeviceState &state, GPU &gpu);

        ~CommandScheduler();

        /**
         * @brief Allocates an existing or new primary command buffer from the pool
         */
        ActiveCommandBuffer AllocateCommandBuffer();

        /**
         * @brief Submits a single command buffer to the GPU queue while queuing it up to be waited on
         * @note The supplied command buffer and cycle **must** be from AllocateCommandBuffer()
         * @note Any cycle submitted via this method does not need to destroy dependencies manually, the waiter thread will handle this
         */
        void SubmitCommandBuffer(const vk::raii::CommandBuffer &commandBuffer, std::shared_ptr<FenceCycle> cycle, span<vk::Semaphore> waitSemaphores = {}, span<vk::Semaphore> signalSemaphore = {});

        /**
         * @brief Submits a command buffer recorded with the supplied function synchronously
         * @param waitSemaphores A span of all (excl fence cycle) semaphores that should be waited on by the GPU before executing the command buffer
         * @param signalSemaphore A span of all semaphores that should be signalled by the GPU after executing the command buffer
         */
        template<typename RecordFunction>
        std::shared_ptr<FenceCycle> Submit(RecordFunction recordFunction, span<vk::Semaphore> waitSemaphores = {}, span<vk::Semaphore> signalSemaphores = {}) {
            auto commandBuffer{AllocateCommandBuffer()};
            try {
                commandBuffer->begin(vk::CommandBufferBeginInfo{
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                });
                recordFunction(*commandBuffer);
                commandBuffer->end();

                auto cycle{commandBuffer.GetFenceCycle()};
                SubmitCommandBuffer(*commandBuffer, cycle, waitSemaphores, signalSemaphores);
                return cycle;
            } catch (...) {
                commandBuffer.GetFenceCycle()->Cancel();
                std::rethrow_exception(std::current_exception());
            }
        }
    };
}
