#include "KSyncObject.h"
#include <os.h>

namespace skyline::kernel::type {
    KSyncObject::KSyncObject(const skyline::DeviceState &state, skyline::kernel::type::KType type) : KObject(state, type) {}

    KSyncObject::threadInfo::threadInfo(pid_t process, handle_t handle) : process(process), handle(handle) {}

    void KSyncObject::Signal() {
        for (const auto &info : waitThreads) {
            state.nce->SetRegister(Wreg::W1, info.handle);
            state.os->processMap.at(info.process)->threadMap.at(info.process)->status = KThread::Status::Runnable;
        }
    }
}
