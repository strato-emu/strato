#include "KSyncObject.h"
#include <os.h>

namespace skyline::kernel::type {
    KSyncObject::KSyncObject(const skyline::DeviceState &state, skyline::kernel::type::KType type) : KObject(state, type) {}

    KSyncObject::threadInfo::threadInfo(pid_t process, u32 index) : process(process), index(index) {}

    void KSyncObject::Signal() {
        for (const auto &info : waitThreads) {
            state.nce->SetRegister(Wreg::W1, info.index);
            state.os->processMap.at(info.process)->threadMap.at(info.process)->status = KThread::Status::Runnable;
        }
        waitThreads.clear();
    }
}
