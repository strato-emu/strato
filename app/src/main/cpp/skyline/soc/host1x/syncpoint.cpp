// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2020 Ryujinx Team and Contributors

#include "syncpoint.h"

namespace skyline::soc::host1x {
    Syncpoint::WaiterHandle Syncpoint::RegisterWaiter(u32 threshold, const std::function<void()> &callback) {
        if (value.load(std::memory_order_acquire) >= threshold) {
            // (Fast path) We don't need to wait on the mutex and can just get away with atomics
            callback();
            return {};
        }

        std::scoped_lock lock(mutex);
        if (value.load(std::memory_order_acquire) >= threshold) {
            callback();
            return {};
        }

        auto it{waiters.begin()};
        while (it != waiters.end() && threshold >= it->threshold)
            it++;
        return waiters.emplace(it, threshold, callback);
    }

    void Syncpoint::DeregisterWaiter(WaiterHandle waiter) {
        std::scoped_lock lock(mutex);
        // We want to ensure the iterator still exists prior to erasing it
        // Otherwise, if an invalid iterator was passed in then it could lead to UB
        // It is important to avoid UB in that case since the deregister isn't called from a locked context
        for (auto it{waiters.begin()}; it != waiters.end(); it++)
            if (it == waiter)
                waiters.erase(it);
    }

    u32 Syncpoint::Increment() {
        auto readValue{value.fetch_add(1, std::memory_order_acq_rel) + 1}; // We don't want to constantly do redundant atomic loads

        std::scoped_lock lock(mutex);
        bool signalCondition{};
        auto it{waiters.begin()};
        while (it != waiters.end() && readValue >= it->threshold) {
            auto &waiter{*it};
            if (waiter.callback)
                waiter.callback();
            else
                signalCondition = true;
            it++;
        }

        waiters.erase(waiters.begin(), it);

        if (signalCondition)
            incrementCondition.notify_all();

        return readValue;
    }

    bool Syncpoint::Wait(u32 threshold, std::chrono::steady_clock::duration timeout) {
        if (value.load(std::memory_order_acquire) >= threshold)
            // (Fast Path) We don't need to wait on the mutex and can just get away with atomics
            return {};

        std::unique_lock lock(mutex);
        auto it{waiters.begin()};
        while (it != waiters.end() && threshold >= it->threshold)
            it++;
        waiters.emplace(it, threshold, nullptr);

        if (timeout == std::chrono::steady_clock::duration::max()) {
            incrementCondition.wait(lock, [&] { return value.load(std::memory_order_relaxed) >= threshold; });
            return true;
        } else {
            return incrementCondition.wait_for(lock, timeout, [&] { return value.load(std::memory_order_relaxed) >= threshold; });
        }
    }
}

