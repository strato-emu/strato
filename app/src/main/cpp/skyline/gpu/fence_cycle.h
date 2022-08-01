// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <common.h>
#include <common/atomic_forward_list.h>

namespace skyline::gpu {
    class CommandScheduler;

    /**
     * @brief A wrapper around a Vulkan Fence which only tracks a single reset -> signal cycle with the ability to attach lifetimes of objects to it
     * @note This provides the guarantee that the fence must be signalled prior to destruction when objects are to be destroyed
     * @note All waits to the fence **must** be done through the same instance of this, the state of the fence changing externally will lead to UB
     */
    struct FenceCycle {
      private:
        std::atomic_flag signalled{}; //!< If the underlying fence has been signalled since the creation of this FenceCycle, this doesn't necessarily mean the dependencies have been destroyed
        std::atomic_flag alreadyDestroyed{}; //!< If the cycle's dependencies are already destroyed, this prevents multiple destructions
        const vk::raii::Device &device;
        vk::Fence fence;

        friend CommandScheduler;

        AtomicForwardList<std::shared_ptr<void>> dependencies; //!< A list of all dependencies on this fence cycle
        AtomicForwardList<std::shared_ptr<FenceCycle>> chainedCycles; //!< A list of all chained FenceCycles, this is used to express multi-fence dependencies

        /**
         * @brief Destroy all the dependencies of this cycle
         * @note We cannot delete the chained cycles associated with this fence as they may be iterated over during the deletion, it is only safe to delete them during the destruction of the cycle
         */
        void DestroyDependencies() {
            if (!alreadyDestroyed.test_and_set(std::memory_order_release))
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
            signalled.test_and_set(std::memory_order_release);
            DestroyDependencies();
        }

        /**
         * @brief Wait on a fence cycle till it has been signalled
         * @param shouldDestroy If true, the dependencies of this cycle will be destroyed after the fence is signalled
         */
        void Wait(bool shouldDestroy = false) {
            if (signalled.test(std::memory_order_consume)) {
                if (shouldDestroy)
                    DestroyDependencies();
                return;
            }

            chainedCycles.Iterate([shouldDestroy](auto &cycle) {
                cycle->Wait(shouldDestroy);
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

            signalled.test_and_set(std::memory_order_release);
            if (shouldDestroy)
                DestroyDependencies();
        }

        /**
         * @brief Wait on a fence cycle with a timeout in nanoseconds
         * @param shouldDestroy If true, the dependencies of this cycle will be destroyed after the fence is signalled
         * @return If the wait was successful or timed out
         */
        bool Wait(i64 timeoutNs, bool shouldDestroy = false) {
            if (signalled.test(std::memory_order_consume)) {
                if (shouldDestroy)
                    DestroyDependencies();
                return true;
            }

            i64 startTime{util::GetTimeNs()}, initialTimeout{timeoutNs};
            if (!chainedCycles.AllOf([&](auto &cycle) {
                if (!cycle->Wait(timeoutNs, shouldDestroy))
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
                signalled.test_and_set(std::memory_order_release);
                if (shouldDestroy)
                    DestroyDependencies();
                return true;
            } else {
                return false;
            }
        }

        bool Wait(std::chrono::duration<i64, std::nano> timeout, bool shouldDestroy = false) {
            return Wait(timeout.count(), shouldDestroy);
        }

        /**
         * @param quick Skips the call to check the fence's status, just checking the signalled flag
         * @return If the fence is signalled currently or not
         */
        bool Poll(bool quick = true, bool shouldDestroy = false) {
            if (signalled.test(std::memory_order_consume)) {
                if (shouldDestroy)
                    DestroyDependencies();
                return true;
            }

            if (quick)
                return false; // We need to return early if we're not waiting on the fence

            if (!chainedCycles.AllOf([=](auto &cycle) { return cycle->Poll(quick, shouldDestroy); }))
                return false;

            auto status{(*device).getFenceStatus(fence, *device.getDispatcher())};
            if (status == vk::Result::eSuccess) {
                signalled.test_and_set(std::memory_order_release);
                if (shouldDestroy)
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
