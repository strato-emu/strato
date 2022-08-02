// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <soc/gm20b/engines/maxwell/types.h>
#include "descriptor_allocator.h"

namespace skyline::gpu {
    DescriptorAllocator::DescriptorSetSlot::DescriptorSetSlot(vk::DescriptorSet descriptorSet) : descriptorSet{descriptorSet} {}

    DescriptorAllocator::DescriptorSetSlot::DescriptorSetSlot(DescriptorAllocator::DescriptorSetSlot &&other) : descriptorSet{other.descriptorSet} {
        other.descriptorSet = nullptr;
    }

    DescriptorAllocator::DescriptorPool::DescriptorPool(const vk::raii::Device &device, const vk::DescriptorPoolCreateInfo &createInfo) : vk::raii::DescriptorPool{device, createInfo}, freeSetCount{createInfo.maxSets} {}

    void DescriptorAllocator::AllocateDescriptorPool() {
        namespace maxwell3d = soc::gm20b::engine::maxwell3d::type; // We use Maxwell3D as reference for base descriptor counts
        using DescriptorSizes = std::array<vk::DescriptorPoolSize, 5>;
        constexpr DescriptorSizes BaseDescriptorSizes{
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::PipelineStageConstantBufferCount,
                .type = vk::DescriptorType::eUniformBuffer,
            },
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::PipelineStageCount * 5,
                .type = vk::DescriptorType::eStorageBuffer,
            },
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::PipelineStageCount * 5,
                .type = vk::DescriptorType::eCombinedImageSampler,
            },
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::PipelineStageCount,
                .type = vk::DescriptorType::eStorageImage,
            },
            vk::DescriptorPoolSize{
                .descriptorCount = maxwell3d::RenderTargetCount,
                .type = vk::DescriptorType::eInputAttachment,
            },
        }; //!< A best approximate ratio of descriptors of each type that may be utilized, the total amount will grow in these ratios

        DescriptorSizes descriptorSizes{BaseDescriptorSizes};
        for (auto &descriptorSize : descriptorSizes)
            descriptorSize.descriptorCount *= descriptorMultiplier;

        pool = std::make_shared<DescriptorPool>(gpu.vkDevice, vk::DescriptorPoolCreateInfo{
            .maxSets = descriptorSetCount,
            .pPoolSizes = descriptorSizes.data(),
            .poolSizeCount = descriptorSizes.size(),
        });
    }

    vk::ResultValue<vk::DescriptorSet> DescriptorAllocator::AllocateVkDescriptorSet(vk::DescriptorSetLayout layout) {
        vk::DescriptorSetAllocateInfo allocateInfo{
            .descriptorPool = **pool,
            .pSetLayouts = &layout,
            .descriptorSetCount = 1,
        };
        vk::DescriptorSet descriptorSet{};

        auto result{(*gpu.vkDevice).allocateDescriptorSets(&allocateInfo, &descriptorSet, *gpu.vkDevice.getDispatcher())};
        return vk::createResultValue(result, descriptorSet, __builtin_FUNCTION(), {
            vk::Result::eSuccess,
            vk::Result::eErrorOutOfPoolMemory,
            vk::Result::eErrorFragmentedPool
        });
    }

    DescriptorAllocator::ActiveDescriptorSet::ActiveDescriptorSet(std::shared_ptr<DescriptorPool> pPool, DescriptorSetSlot *slot) : pool{std::move(pPool)}, slot{slot} {
        pool->freeSetCount--;
    }

    DescriptorAllocator::ActiveDescriptorSet::ActiveDescriptorSet(DescriptorAllocator::ActiveDescriptorSet &&other) noexcept {
        pool = std::move(other.pool);
        slot = std::exchange(other.slot, nullptr);
    }

    DescriptorAllocator::ActiveDescriptorSet::~ActiveDescriptorSet() {
        if (slot) {
            slot->active.clear(std::memory_order_release);
            pool->freeSetCount++;
        }
    }

    DescriptorAllocator::DescriptorAllocator(GPU &gpu) : gpu{gpu} {
        AllocateDescriptorPool();
    }

    DescriptorAllocator::ActiveDescriptorSet DescriptorAllocator::AllocateSet(vk::DescriptorSetLayout layout) {
        std::scoped_lock allocatorLock{mutex};

        auto it{pool->layoutSlots.find(layout)};
        vk::Result lastResult{};
        if (it != pool->layoutSlots.end()) {
            auto &slots{it->second};
            for (auto &slot : it->second)
                if (!slot.active.test_and_set(std::memory_order_acq_rel))
                    return ActiveDescriptorSet{pool, &slot};

            // If we couldn't find an available slot, we need to allocate a new one
            auto set{AllocateVkDescriptorSet(layout)};
            if (set.result == vk::Result::eSuccess) {
                auto &slot{slots.emplace_back(set.value)};
                return ActiveDescriptorSet{pool, &slot};
            } else {
                lastResult = set.result;
            }
        } else {
            // If we couldn't find a layout, we need to allocate a new one
            auto set{AllocateVkDescriptorSet(layout)};
            if (set.result == vk::Result::eSuccess) {
                auto &layoutSlots{pool->layoutSlots.try_emplace(layout).first->second};
                return ActiveDescriptorSet{pool, &layoutSlots.emplace_back(set.value)};
            } else {
                lastResult = set.result;
            }
        }

        while (true) {
            // We attempt to modify the pool based on the last result
            if (lastResult == vk::Result::eErrorOutOfPoolMemory) {
                if (pool->freeSetCount == 0)
                    // The amount of maximum descriptor sets is insufficient
                    descriptorSetCount += DescriptorSetCountIncrement;
                else
                    // The amount of maximum descriptors is insufficient
                    descriptorMultiplier++;
                AllocateDescriptorPool();
            } else if (lastResult == vk::Result::eErrorFragmentedPool) {
                AllocateDescriptorPool(); // If the pool is fragmented, we reallocate without increasing the size
            }

            // Try to allocate a new layout
            auto set{AllocateVkDescriptorSet(layout)};
            if (set.result == vk::Result::eSuccess) {
                auto &layoutSlots{pool->layoutSlots.try_emplace(layout).first->second};
                return ActiveDescriptorSet{pool, &layoutSlots.emplace_back(set.value)};
            } else {
                lastResult = set.result;
            }
        }
    }
}
