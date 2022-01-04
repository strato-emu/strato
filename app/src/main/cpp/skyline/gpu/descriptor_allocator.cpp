// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <soc/gm20b/engines/maxwell/types.h>
#include "descriptor_allocator.h"

namespace skyline::gpu {
    DescriptorAllocator::DescriptorPool::DescriptorPool(const vk::raii::Device &device, const vk::DescriptorPoolCreateInfo &createInfo) : vk::raii::DescriptorPool(device, createInfo), freeSetCount(createInfo.maxSets) {}

    void DescriptorAllocator::AllocateDescriptorPool() {
        namespace maxwell3d = soc::gm20b::engine::maxwell3d::type; // We use Maxwell3D as reference for base descriptor counts
        using DescriptorSizes = std::array<vk::DescriptorPoolSize, 2>;
        constexpr DescriptorSizes BaseDescriptorSizes{
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::PipelineStageConstantBufferCount,
                .type = vk::DescriptorType::eUniformBuffer,
            },
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::PipelineStageCount * 20,
                .type = vk::DescriptorType::eCombinedImageSampler,
            },
        };

        DescriptorSizes descriptorSizes{BaseDescriptorSizes};
        for (auto &descriptorSize : descriptorSizes)
            descriptorSize.descriptorCount *= descriptorMultiplier;

        pool = std::make_shared<DescriptorPool>(gpu.vkDevice, vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = descriptorSetCount,
            .pPoolSizes = descriptorSizes.data(),
            .poolSizeCount = descriptorSizes.size(),
        });
    }

    DescriptorAllocator::ActiveDescriptorSet::ActiveDescriptorSet(std::shared_ptr<DescriptorPool> pPool, vk::DescriptorSet set) : pool(std::move(pPool)), DescriptorSet(set) {
        pool->freeSetCount--;
    }

    DescriptorAllocator::ActiveDescriptorSet::ActiveDescriptorSet(DescriptorAllocator::ActiveDescriptorSet &&other) noexcept {
        pool = std::move(other.pool);
        static_cast<vk::DescriptorSet &>(*this) = std::exchange(static_cast<vk::DescriptorSet &>(other), vk::DescriptorSet{});
    }

    DescriptorAllocator::ActiveDescriptorSet::~ActiveDescriptorSet() {
        if (static_cast<vk::DescriptorSet &>(*this)) {
            std::scoped_lock lock(*pool);
            pool->getDevice().freeDescriptorSets(**pool, 1, this, *pool->getDispatcher());
            pool->freeSetCount++;
        }
    }

    DescriptorAllocator::DescriptorAllocator(GPU &gpu) : gpu(gpu) {
        AllocateDescriptorPool();
    }

    DescriptorAllocator::ActiveDescriptorSet DescriptorAllocator::AllocateSet(vk::DescriptorSetLayout layout) {
        std::scoped_lock allocatorLock(mutex);

        while (true) {
            std::scoped_lock poolLock(*pool);

            vk::DescriptorSetAllocateInfo allocateInfo{
                .descriptorPool = **pool,
                .pSetLayouts = &layout,
                .descriptorSetCount = 1,
            };
            vk::DescriptorSet set{};

            auto result{(*gpu.vkDevice).allocateDescriptorSets(&allocateInfo, &set, *gpu.vkDevice.getDispatcher())};
            if (result == vk::Result::eSuccess) {
                return ActiveDescriptorSet(pool, set);
            } else if (result == vk::Result::eErrorOutOfPoolMemory) {
                if (pool->freeSetCount == 0)
                    // The amount of maximum descriptor sets is insufficient
                    descriptorSetCount += DescriptorSetCountIncrement;
                else
                    // The amount of maximum descriptors is insufficient
                    descriptorMultiplier++;
                AllocateDescriptorPool();
                continue; // Attempt to allocate again with the new pool
            } else if (result == vk::Result::eErrorFragmentedPool) {
                AllocateDescriptorPool(); // If the pool is fragmented, we reallocate without increasing the size
                continue;
            } else {
                vk::throwResultException(result, __builtin_FUNCTION());
            }
        }
    }
}
