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
        FenceCycle *cycle{}; //!< A pointer to the fence cycle owning this, used to prevent attaching to the same cycle multiple times but should never be used to access the object due to not being an owning reference nor nullified on the destruction of the owning cycle
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
        std::shared_ptr<FenceCycleDependency> list{};

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
         * @brief Signals this fence regardless of if the underlying fence has been signalled or not
         */
        void Cancel() {
            if (!signalled.test_and_set(std::memory_order_release))
                DestroyDependencies();
        }

        /**
         * @brief Wait on a fence cycle till it has been signalled
         */
        void Wait() {
            if (signalled.test(std::memory_order_consume))
                return;

            vk::Result waitResult;
            while ((waitResult = (*device).waitForFences(1, &fence, false, std::numeric_limits<u64>::max(), *device.getDispatcher())) != vk::Result::eSuccess) {
                if (waitResult == vk::Result::eTimeout)
                    // Retry if the waiting time out
                    continue;

                if (waitResult == vk::Result::eErrorInitializationFailed)
                    // eErrorInitializationFailed occurs on Mali GPU drivers due to them using the ppoll() syscall which isn't correctly restarted after a signal, we need to manually retry waiting in that case
                    continue;

                throw exception("An error occurred while waiting for fence 0x{:X}: {}", static_cast<VkFence>(fence), vk::to_string(waitResult));
            }

            if (!signalled.test_and_set(std::memory_order_release))
                DestroyDependencies();
        }

        /**
         * @brief Wait on a fence cycle with a timeout in nanoseconds
         * @return If the wait was successful or timed out
         */
        bool Wait(std::chrono::duration<u64, std::nano> timeout) {
            if (signalled.test(std::memory_order_consume))
                return true;
            if ((*device).waitForFences(1, &fence, false, std::numeric_limits<u64>::max(), *device.getDispatcher()) == vk::Result::eSuccess) {
                if (!signalled.test_and_set(std::memory_order_release))
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
            auto status{(*device).getFenceStatus(fence, *device.getDispatcher())};
            if (status == vk::Result::eSuccess) {
                if (!signalled.test_and_set(std::memory_order_release))
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
            if (dependency->cycle != this && !signalled.test(std::memory_order_consume)) {
                // Only try to insert if the object isn't already owned by this fence cycle and it hasn't been signalled yet
                dependency->next = std::atomic_load_explicit(&list, std::memory_order_acquire);

                do {
                    if (dependency->next == nullptr && signalled.test(std::memory_order_consume)) {
                        // `list` will be nullptr after being signalled, dependencies will not be destroyed and we need to do so ourselves
                        dependency->next.reset(); // We need to reset the dependency so it doesn't incorrectly extend the lifetime of any resources
                        return;
                    }
                } while (!std::atomic_compare_exchange_strong_explicit(&list, &dependency->next, dependency, std::memory_order_release, std::memory_order_acquire));

                dependency->cycle = this;
            }
        }

        /**
         * @brief A version of AttachObject optimized for several objects being attached at once
         */
        void AttachObjects(std::initializer_list<std::shared_ptr<FenceCycleDependency>> dependencies) {
            if (!signalled.test(std::memory_order_consume)) {
                {
                    auto it{dependencies.begin()}, next{std::next(it)};
                    if (it != dependencies.end()) {
                        // Only try to insert if the object isn't already owned by this fence cycle
                        for (; next != dependencies.end(); next++) {
                            if ((*it)->cycle != this) {
                                (*it)->next = *next;
                                (*it)->cycle = this;
                            }
                            it = next;
                        }
                    }
                }

                auto &dependency{*dependencies.begin()};
                auto &lastDependency{*std::prev(dependencies.end())};
                lastDependency->next = std::atomic_load_explicit(&list, std::memory_order_acquire);

                do {
                    if (lastDependency->next == nullptr && signalled.test(std::memory_order_consume)) {
                        // `list` will be nullptr after being signalled, dependencies will not be destroyed and we need to do so ourselves

                        auto current{dependency->next}; // We need to reset any chained dependencies before exiting
                        while (current) {
                            std::shared_ptr<FenceCycleDependency> next{};
                            next.swap(current->next);
                            current.swap(next);
                        }

                        return;
                    }
                } while (!std::atomic_compare_exchange_strong_explicit(&list, &dependency->next, dependency, std::memory_order_release, std::memory_order_acquire));
            }
        }

        template<typename... Dependencies>
        void AttachObjects(Dependencies &&... dependencies) {
            AttachObjects(std::initializer_list<std::shared_ptr<FenceCycleDependency>>{std::forward<Dependencies>(dependencies)...});
        }
    };
}
