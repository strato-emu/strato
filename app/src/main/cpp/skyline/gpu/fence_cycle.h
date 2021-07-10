// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <forward_list>
#include <vulkan/vulkan_raii.hpp>
#include <common.h>

namespace skyline::gpu {
    struct FenceCycle;

    /**
     * @brief Any object whose lifetime can be attached to a fence cycle needs to inherit this class
     */
    struct FenceCycleDependency {
      private:
        std::shared_ptr<FenceCycleDependency> next{}; //!< A shared pointer to the next dependendency to form a linked list
        friend FenceCycle;
    };

    /**
     * @brief A wrapper around a Vulkan Fence which only tracks a single reset -> signal cycle with the ability to attach lifetimes of objects to it
     * @note This provides the guarantee that the fence must be signalled prior to destruction when objects are to be destroyed
     * @note All waits to the fence **must** be done through the same instance of this, the state of the fence changing externally will lead to UB
     */
    struct FenceCycle {
      private:
        std::atomic_flag signalled;
        const vk::raii::Device &device;
        vk::Fence fence;
        std::shared_ptr<FenceCycleDependency> list;

        /**
         * @brief Sequentially iterate through the shared_ptr linked list of dependencies and reset all pointers in a thread-safe atomic manner
         * @note We cannot simply nullify the base pointer of the list as a false dependency chain is maintained between the objects when retained exteranlly
         */
        void DestroyDependencies() {
            auto current{std::atomic_exchange_explicit(&list, std::shared_ptr<FenceCycleDependency>{}, std::memory_order_acquire)};
            while (current) {
                std::shared_ptr<FenceCycleDependency> next{};
                next.swap(current->next);
                current.swap(next);
            }
        }

      public:
        FenceCycle(const vk::raii::Device &device, vk::Fence fence) : signalled(false), device(device), fence(fence) {
            device.resetFences(fence);
        }

        ~FenceCycle() {
            Wait();
        }

        /**
         * @brief Wait on a fence cycle till it has been signalled
         */
        void Wait() {
            if (signalled.test(std::memory_order_consume))
                return;
            while (device.waitForFences(fence, false, std::numeric_limits<u64>::max()) != vk::Result::eSuccess);
            if (signalled.test_and_set(std::memory_order_release))
                DestroyDependencies();
        }

        /**
         * @brief Wait on a fence cycle with a timeout in nanoseconds
         * @return If the wait was successful or timed out
         */
        bool Wait(std::chrono::duration<u64, std::nano> timeout) {
            if (signalled.test(std::memory_order_consume))
                return true;
            if (device.waitForFences(fence, false, timeout.count()) == vk::Result::eSuccess) {
                if (signalled.test_and_set(std::memory_order_release))
                    DestroyDependencies();
                return true;
            } else {
                return false;
            }
        }

        /**
         * @return If the fence is signalled currently or not
         */
        bool Poll() {
            if (signalled.test(std::memory_order_consume))
                return true;
            if ((*device).getFenceStatus(fence, *device.getDispatcher()) == vk::Result::eSuccess) {
                if (signalled.test_and_set(std::memory_order_release))
                    DestroyDependencies();
                return true;
            } else {
                return false;
            }
        }

        /**
         * @brief Attach the lifetime of an object to the fence being signalled
         */
        void AttachObject(const std::shared_ptr<FenceCycleDependency> &dependency) {
            if (!signalled.test(std::memory_order_consume)) {
                std::shared_ptr<FenceCycleDependency> next{std::atomic_load_explicit(&list, std::memory_order_consume)};
                do {
                    dependency->next = next;
                    if (!next && signalled.test(std::memory_order_consume))
                        return;
                } while (std::atomic_compare_exchange_strong_explicit(&list, &next, dependency, std::memory_order_release, std::memory_order_consume));
            }
        }

        /**
         * @brief A version of AttachObject optimized for several objects being attached at once
         */
        void AttachObjects(std::initializer_list<std::shared_ptr<FenceCycleDependency>> dependencies) {
            if (!signalled.test(std::memory_order_consume)) {
                auto it{dependencies.begin()}, next{std::next(it)};
                if (it != dependencies.end()) {
                    for (; next != dependencies.end(); next++) {
                        (*it)->next = *next;
                        it = next;
                    }
                }
                AttachObject(*dependencies.begin());
            }
        }

        template<typename... Dependencies>
        void AttachObjects(Dependencies &&... dependencies) {
            AttachObjects(std::initializer_list<std::shared_ptr<FenceCycleDependency>>{std::forward<Dependencies>(dependencies)...});
        }
    };
}
