// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "syncpoint.h"

namespace skyline::soc::host1x {
    u64 Syncpoint::RegisterWaiter(u32 threshold, const std::function<void()> &callback) {
        if (value >= threshold) {
            callback();
            return 0;
        }

        std::lock_guard guard(waiterLock);
        waiterMap.emplace(nextWaiterId, Waiter{threshold, callback});

        return nextWaiterId++;
    }

    void Syncpoint::DeregisterWaiter(u64 id) {
        std::lock_guard guard(waiterLock);
        waiterMap.erase(id);
    }

    u32 Syncpoint::Increment() {
        value++;

        std::lock_guard guard(waiterLock);
        std::erase_if(waiterMap, [this](const auto &entry) {
            if (value >= entry.second.threshold) {
                entry.second.callback();
                return true;
            } else {
                return false;
            }
        });

        return value;
    }

    bool Syncpoint::Wait(u32 threshold, std::chrono::steady_clock::duration timeout) {
        std::mutex mtx;
        std::condition_variable cv;
        bool flag{};

        if (timeout == std::chrono::steady_clock::duration::max())
            timeout = std::chrono::seconds(1);

        if (!RegisterWaiter(threshold, [&cv, &mtx, &flag] {
            std::unique_lock lock(mtx);
            flag = true;
            lock.unlock();
            cv.notify_all();
        })) {
            return true;
        }

        std::unique_lock lock(mtx);
        return cv.wait_for(lock, timeout, [&flag] { return flag; });
    }
}

