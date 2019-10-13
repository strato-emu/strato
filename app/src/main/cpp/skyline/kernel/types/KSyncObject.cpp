#include "KSyncObject.h"
#include <os.h>

namespace skyline::kernel::type {
    KSyncObject::KSyncObject(const skyline::DeviceState &state, skyline::kernel::type::KType type) : KObject(state, type) {}

    void KSyncObject::Signal() {
        for(const auto& pid : waitThreads) {
            state.os->processMap.at(pid)->threadMap.at(pid)->status = KThread::ThreadStatus::Runnable;
        }
    }
}
