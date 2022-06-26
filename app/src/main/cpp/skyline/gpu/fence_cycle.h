// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <common.h>
#include <common/atomic_forward_list.h>

namespace skyline::gpu {
    struct FenceCycle;

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

        AtomicForwardList<std::shared_ptr<void>> dependencies; //!< A list of all dependencies on this fence cycle
        AtomicForwardList<std::shared_ptr<FenceCycle>> chainedCycles; //!< A list of all chained FenceCycles, this is used to express multi-fence dependencies

        /**
         * @brief Sequentially iterate through the shared_ptr linked list of dependencies and reset all pointers in a thread-safe atomic manner
         * @note We cannot simply nullify the base pointer of the list as a false dependency chain is maintained between the objects when retained externally
         */
        void DestroyDependencies() {
            dependencies.Clear();
        }

      public:
        FenceCycle(const vk::raii::Device &device, vk::Fence fence) : signalled{false}, device{device}, fence{fence} {
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

            chainedCycles.Iterate([](auto &cycle) {
                cycle->Wait();
            });

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
        bool Wait(i64 timeoutNs) {
            if (signalled.test(std::memory_order_consume))
                return true;

            i64 startTime{util::GetTimeNs()}, initialTimeout{timeoutNs};
            if (!chainedCycles.AllOf([&](auto &cycle) {
                if (!cycle->Wait(timeoutNs))
                    return false;
                timeoutNs = std::max<i64>(0, initialTimeout - (util::GetTimeNs() - startTime));
                return true;
            }))
                return false;

            vk::Result waitResult;
            while ((waitResult = (*device).waitForFences(1, &fence, false, static_cast<u64>(timeoutNs), *device.getDispatcher())) != vk::Result::eSuccess) {
                if (waitResult == vk::Result::eTimeout)
                    break;

                if (waitResult == vk::Result::eErrorInitializationFailed) {
                    timeoutNs = std::max<i64>(0, initialTimeout - (util::GetTimeNs() - startTime));
                    continue;
                }

                throw exception("An error occurred while waiting for fence 0x{:X}: {}", static_cast<VkFence>(fence), vk::to_string(waitResult));
            }

            if (waitResult == vk::Result::eSuccess) {
                if (!signalled.test_and_set(std::memory_order_release))
                    DestroyDependencies();
                return true;
            } else {
                return false;
            }
        }

        bool Wait(std::chrono::duration<i64, std::nano> timeout) {
            return Wait(timeout.count());
        }

        /**
         * @return If the fence is signalled currently or not
         */
        bool Poll() {
            if (signalled.test(std::memory_order_consume))
                return true;

            if (!chainedCycles.AllOf([](auto &cycle) { return cycle->Poll(); }))
                return false;

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
        void AttachObject(const std::shared_ptr<void> &dependency) {
            if (!signalled.test(std::memory_order_consume))
                dependencies.Append(dependency);
        }

        /**
         * @brief A version of AttachObject optimized for several objects being attached at once
         */
        template<typename... Dependencies>
        void AttachObjects(Dependencies &&... pDependencies) {
            if (!signalled.test(std::memory_order_consume))
                dependencies.Append(pDependencies...);
        }

        /**
         * @brief Chains another cycle to this cycle, this cycle will not be signalled till the supplied cycle is signalled
         * @param cycle The cycle to chain to this one, this is nullable and this function will be a no-op if this is nullptr
         */
        void ChainCycle(const std::shared_ptr<FenceCycle> &cycle) {
            if (cycle && !signalled.test(std::memory_order_consume) && cycle.get() != this && cycle->Poll())
                chainedCycles.Append(cycle); // If the cycle isn't the current cycle or already signalled, we need to chain it
        }
    };
}
