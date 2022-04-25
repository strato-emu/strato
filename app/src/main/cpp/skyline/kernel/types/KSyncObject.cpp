// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "KSyncObject.h"
#include "KThread.h"

namespace skyline::kernel::type {
    void KSyncObject::Signal() {
        std::scoped_lock lock{syncObjectMutex};
        signalled = true;
        for (auto &waiter : syncObjectWaiters) {
            if (waiter->isCancellable) {
                waiter->isCancellable = false;
                waiter->wakeObject = this;
                state.scheduler->InsertThread(waiter);
            }
        }
    }

    bool KSyncObject::ResetSignal() {
        std::scoped_lock lock{syncObjectMutex};
        if (signalled) [[likely]] {
            signalled = false;
            return true;
        }
        return false;
    }
}
