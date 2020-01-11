#include <sys/resource.h>
#include "KThread.h"
#include "KProcess.h"
#include <nce.h>

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, handle_t handle, pid_t self_pid, u64 entryPoint, u64 entryArg, u64 stackTop, u64 tls, u8 priority, KProcess *parent, std::shared_ptr<type::KSharedMemory> &tlsMemory) : handle(handle), pid(self_pid), entryPoint(entryPoint), entryArg(entryArg), stackTop(stackTop), tls(tls), priority(priority), parent(parent), ctxMemory(tlsMemory), KSyncObject(state,
                                                                                                                                                                                                                                                                                                                                                                                                      KType::KThread) {
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        Kill();
    }

    void KThread::Start() {
        if (status == Status::Created) {
            if (pid == parent->pid)
                parent->status = KProcess::Status::Started;
            status = Status::Running;
            state.nce->StartThread(entryArg, handle, parent->threadMap.at(pid));
        }
    }

    void KThread::Kill() {
        if (status != Status::Dead) {
            status = Status::Dead;
            Signal();
        }
    }

    void KThread::UpdatePriority(u8 priority) {
        this->priority = priority;
        auto liPriority = static_cast<int8_t>(constant::PriorityAn.first + ((static_cast<float>(constant::PriorityAn.second - constant::PriorityAn.first) / static_cast<float>(constant::PriorityNin.second - constant::PriorityNin.first)) * (static_cast<float>(priority) - constant::PriorityNin.first))); // Resize range PriorityNin (Nintendo Priority) to PriorityAn (Android Priority)
        if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), liPriority) == -1)
            throw exception("Couldn't set process priority to {} for PID: {}", liPriority, pid);
    }
}
