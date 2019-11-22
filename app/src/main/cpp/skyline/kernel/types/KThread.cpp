#include <sys/resource.h>
#include "KThread.h"
#include "KProcess.h"
#include <nce.h>

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, handle_t handle, pid_t self_pid, u64 entryPoint, u64 entryArg, u64 stackTop, u64 tls, u8 priority, KProcess *parent) : handle(handle), pid(self_pid), entryPoint(entryPoint), entryArg(entryArg), stackTop(stackTop), tls(tls), priority(priority), parent(parent), KSyncObject(state, KType::KThread) {
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        Kill();
    }

    void KThread::Start() {
        if(status == Status::Created) {
            if (pid == parent->mainThread)
                parent->status = KProcess::Status::Started;
            status = Status::Running;
            state.nce->StartProcess(entryPoint, entryArg, stackTop, handle, pid);
        }
    }

    void KThread::Kill() {
        if(status != Status::Dead) {
            status = Status::Dead;
            kill(pid, SIGKILL);
            Signal();
        }
    }

    void KThread::UpdatePriority(u8 priority) {
        this->priority = priority;
        auto liPriority = static_cast<int8_t>(constant::PriorityAn.first + ((static_cast<float>(constant::PriorityAn.second - constant::PriorityAn.first) / static_cast<float>(constant::PriorityNin.second - constant::PriorityNin.first)) * (static_cast<float>(priority) - constant::PriorityNin.first))); // Resize range PriorityNin (Nintendo Priority) to PriorityAn (Android Priority)
        if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), liPriority) == -1)
            throw exception("Couldn't set process priority to {} for PID: {}", liPriority, pid);
    }

    void KThread::Sleep() {
        if (status == Status::Running) {
            status = Status::Sleeping;
            timeout = 0;
        }
    }

    void KThread::WakeUp() {
        if (status == Status::Sleeping)
            status = Status::Runnable;
    }

    void KThread::ClearWaitObjects() {
        for (auto &object : waitObjects) {
            auto iter = object->waitThreads.begin();
            while (iter != object->waitThreads.end()) {
                if (iter->process == pid) {
                    object->waitThreads.erase(iter);
                    break;
                }
                iter++;
            }
        }
        waitObjects.clear();
    }
}
