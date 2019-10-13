#include <sys/resource.h>
#include "KThread.h"
#include "KProcess.h"
#include <nce.h>

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, handle_t handle, pid_t self_pid, u64 entryPoint, u64 entryArg, u64 stackTop, u64 tls, u8 priority, KProcess *parent) : handle(handle), pid(self_pid), entryPoint(entryPoint), entryArg(entryArg), stackTop(stackTop), tls(tls), priority(priority), parent(parent), KSyncObject(state, KType::KThread) {
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        kill(pid, SIGKILL);
    }

    void KThread::Start() {
        if (pid == parent->mainThread)
            parent->status = KProcess::ProcessStatus::Started;
        status = ThreadStatus::Running;
        state.nce->StartProcess(entryPoint, entryArg, stackTop, handle, pid);
    }

    void KThread::UpdatePriority(u8 priority) {
        this->priority = priority;
        auto liPriority = static_cast<int8_t>(constant::PriorityAn.first + ((static_cast<float>(constant::PriorityAn.second - constant::PriorityAn.first) / static_cast<float>(constant::PriorityNin.second - constant::PriorityNin.first)) * (static_cast<float>(priority) - constant::PriorityNin.first))); // Resize range PriorityNin (Nintendo Priority) to PriorityAn (Android Priority)
        if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), liPriority) == -1)
            throw exception(fmt::format("Couldn't set process priority to {} for PID: {}", liPriority, pid));
    }
}
