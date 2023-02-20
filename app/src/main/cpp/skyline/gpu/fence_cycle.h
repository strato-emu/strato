// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <condition_variable>
#include <vulkan/vulkan_raii.hpp>
#include <common.h>
#include <common/spin_lock.h>
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
        std::recursive_timed_mutex mutex;
        std::condition_variable_any submitCondition;
        bool submitted{}; //!< If the fence has been submitted to the GPU
        vk::Fence fence;
        vk::Semaphore semaphore; //!< Semaphore that will be signalled upon GPU completion of the fence
        bool semaphoreSubmitWait{}; //!< If the semaphore needs to be waited on (on GPU) before the fence's command buffer begins. Used to ensure fences that wouldn't otherwise be unsignalled are unsignalled
        bool nextSemaphoreSubmitWait{true}; //!< If the next fence cycle created from this one after it's signalled should wait on the semaphore to unsignal it
        std::shared_ptr<FenceCycle> semaphoreUnsignalCycle{}; //!< If the semaphore is used on the GPU, the cycle for the submission that uses it, so it can be waited on before the fence is signalled to ensure the semaphore is unsignalled

        friend CommandScheduler;

        AtomicForwardList<std::shared_ptr<void>> dependencies; //!< A list of all dependencies on this fence cycle
        AtomicForwardList<std::shared_ptr<FenceCycle>> chainedCycles; //!< A list of all chained FenceCycles, this is used to express multi-fence dependencies
        SharedSpinLock chainMutex;

        /**
         * @brief Destroy all the dependencies of this cycle
         */
        void DestroyDependencies() {
            if (!alreadyDestroyed.test_and_set(std::memory_order_release)) {
                dependencies.Clear();
                semaphoreUnsignalCycle = {};
                std::scoped_lock lock{chainMutex};
                chainedCycles.Clear();
            }
        }

      public:
        FenceCycle(const vk::raii::Device &device, vk::Fence fence, vk::Semaphore semaphore, bool signalled = false) : signalled{signalled}, device{device}, fence{fence}, semaphore{semaphore}, nextSemaphoreSubmitWait{!signalled} {
            if (!signalled)
                device.resetFences(fence);
        }

        explicit FenceCycle(const FenceCycle &cycle) : signalled{false}, device{cycle.device}, fence{cycle.fence}, semaphore{cycle.semaphore}, semaphoreSubmitWait{cycle.nextSemaphoreSubmitWait} {
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
         * @brief Executes a function with the fence locked to record a usage of its semaphore, if no semaphore can be provided then a CPU-side wait will be performed instead
         */
        std::shared_ptr<FenceCycle> RecordSemaphoreWaitUsage(std::function<std::shared_ptr<FenceCycle>(vk::Semaphore sema)> &&func) {
            // We can't submit any semaphore waits until the signal has been submitted, so do that first
            WaitSubmit();

            std::unique_lock lock{mutex};

            // If we already have a semaphore usage, just wait on the fence since we can't wait on it twice and have no way to add one after the fact
            if (semaphoreUnsignalCycle) {
                // Safe to unlock since semaphoreUnsignalCycle can never be reset
                lock.unlock();

                Wait();
                return func({});
            }

            // If we're already signalled then there's no need to wait on the semaphore
            if (signalled.test(std::memory_order_relaxed))
                return func({});

            semaphoreUnsignalCycle = func(semaphore);
            nextSemaphoreSubmitWait = false; // We don't need a semaphore wait on the next fence cycle to unsignal the semaphore anymore as the usage will do that
            return semaphoreUnsignalCycle;
        }

        /**
         * @brief Waits for submission of the command buffer associated with this cycle to the GPU
         */
        void WaitSubmit() {
            if (signalled.test(std::memory_order_consume))
                return;

            std::unique_lock lock{mutex};
            if (submitted)
                return;

            if (signalled.test(std::memory_order_consume))
                return;

            lock.unlock();
            {
                std::shared_lock chainLock{chainMutex};

                chainedCycles.Iterate([&](const auto &cycle) {
                    cycle->WaitSubmit();
                });
            }
            lock.lock();

            submitCondition.wait(lock, [this] { return submitted; });
        }

        /**
         * @brief Wait on a fence cycle till it has been signalled
         * @param shouldDestroy If true, the dependencies of this cycle will be destroyed after the fence is signalled
         */
        void Wait(bool shouldDestroy = false) {
            if (signalled.test(std::memory_order_consume)) {
                if (shouldDestroy) {
                    std::unique_lock lock{mutex};
                    DestroyDependencies();
                }
                return;
            }

            {
                std::shared_lock lock{chainMutex};
                chainedCycles.Iterate([shouldDestroy](auto &cycle) {
                    cycle->Wait(shouldDestroy);
                });
            }

            std::unique_lock lock{mutex};

            submitCondition.wait(lock, [&] { return submitted; });

            if (signalled.test(std::memory_order_relaxed)) {
                if (shouldDestroy)
                    DestroyDependencies();
                return;
            }

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

            if (semaphoreUnsignalCycle)
                semaphoreUnsignalCycle->Wait();

            signalled.test_and_set(std::memory_order_relaxed);
            if (shouldDestroy)
                DestroyDependencies();
        }

        /**
         * @param quick Skips the call to check the fence's status, just checking the signalled flag
         * @return If the fence is signalled currently or not
         */
        bool Poll(bool quick = true, bool shouldDestroy = false) {
            if (signalled.test(std::memory_order_consume)) {
                if (shouldDestroy) {
                    std::unique_lock lock{mutex, std::try_to_lock};
                    if (!lock)
                        return false;

                    DestroyDependencies();
                }
                return true;
            }

            if (quick)
                return false; // We need to return early if we're not waiting on the fence

            {
                std::shared_lock lock{chainMutex, std::try_to_lock};
                if (!lock)
                    return false;

                if (!chainedCycles.AllOf([=](auto &cycle) { return cycle->Poll(quick, shouldDestroy); }))
                    return false;
            }

            std::unique_lock lock{mutex, std::try_to_lock};
            if (!lock)
                return false;

            if (signalled.test(std::memory_order_relaxed)) {
                if (shouldDestroy)
                    DestroyDependencies();
                return true;
            }

            if (!submitted)
                return false;

            auto status{(*device).getFenceStatus(fence, *device.getDispatcher())};
            if (status == vk::Result::eSuccess) {
                if (semaphoreUnsignalCycle && !semaphoreUnsignalCycle->Poll())
                    return false;

                signalled.test_and_set(std::memory_order_relaxed);
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
            if (cycle && !signalled.test(std::memory_order_consume) && cycle.get() != this && !cycle->Poll()) {
                std::shared_lock lock{chainMutex};
                chainedCycles.Append(cycle); // If the cycle isn't the current cycle or already signalled, we need to chain it
            }
        }

        /**
         * @brief Notifies all waiters that the command buffer associated with this cycle has been submitted
         */
        void NotifySubmitted() {
            std::scoped_lock lock{mutex};
            submitted = true;
            submitCondition.notify_all();
        }
    };
}
