// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "fence_cycle.h"

namespace skyline::gpu {
    /**
     * @brief A dynamic descriptor set allocator with internal resizing of the descriptor pool to size up to allocation demand
     */
    class DescriptorAllocator {
      private:
        GPU &gpu;
        std::mutex mutex; //!< Synchronizes the creation and replacement of the pool object

        static constexpr u32 DescriptorSetCountIncrement{64}; //!< The amount of descriptor sets that we allocate in increments of
        u32 descriptorSetCount{DescriptorSetCountIncrement}; //!< The maximum amount of descriptor sets in the pool
        u32 descriptorMultiplier{1}; //!< A multiplier for the maximum amount of descriptors in the pool

        /**
         * @brief A lockable VkDescriptorPool for maintaining external synchronization requirements
         */
        struct DescriptorPool : public std::mutex, public vk::raii::DescriptorPool {
            u64 freeSetCount{}; //!< The amount of sets free to allocate from this pool

            DescriptorPool(vk::raii::Device const &device, vk::DescriptorPoolCreateInfo const &createInfo);
        };

        std::shared_ptr<DescriptorPool> pool; //!< The current pool used by any allocations in the class, replaced when an error is ran into

        /**
         * @brief (Re-)Allocates the descriptor pool with the current multiplier applied to the descriptor counts and the current descriptor set count
         * @note `DescriptorAllocator::mutex` **must** be locked prior to calling this
         */
        void AllocateDescriptorPool();

      public:
        /**
         * @brief A RAII-bound descriptor set that automatically frees resources into the pool on destruction while respecting external synchronization requirements
         */
        struct ActiveDescriptorSet : public vk::DescriptorSet {
          private:
            friend DescriptorAllocator;
            std::shared_ptr<DescriptorPool> pool;

            /**
             * @note The supplied pool **must** be locked prior to calling this
             */
            ActiveDescriptorSet(std::shared_ptr<DescriptorPool> pool, vk::DescriptorSet set);

          public:
            ~ActiveDescriptorSet();
        };

        DescriptorAllocator(GPU &gpu);

        /**
         * @note It is UB to allocate a set with a descriptor type that isn't in the pool as defined in AllocateDescriptorPool
         */
        ActiveDescriptorSet AllocateSet(vk::DescriptorSetLayout layout);
    };
}
