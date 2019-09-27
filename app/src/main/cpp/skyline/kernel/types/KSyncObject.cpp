#include "KSyncObject.h"
#include "../../os.h"

namespace skyline::kernel::type {
    KSyncObject::KSyncObject(skyline::handle_t handle, pid_t pid, const skyline::DeviceState &state, skyline::kernel::type::KType type) : KObject(handle, pid, state, type) {}

    void KSyncObject::Signal() {
        for (auto&[tid, thread] : state.os->threadMap.at(ownerPid)->threadMap) {
            if (thread->status == type::KThread::ThreadStatus::Waiting) {
                for (auto &waitHandle : thread->waitHandles) {
                    if (handle == waitHandle) {
                        thread->status = type::KThread::ThreadStatus::Runnable;
                        state.nce->SetRegister(Wreg::W0, constant::status::Success, thread->pid);
                        state.nce->SetRegister(Wreg::W1, handle, thread->pid);
                    }
                }
            }
        }
    }
}
