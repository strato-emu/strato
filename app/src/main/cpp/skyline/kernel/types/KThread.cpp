// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/resource.h>
#include <nce.h>
#include "KThread.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, KHandle handle, pid_t selfTid, u64 entryPoint, u64 entryArg, u64 stackTop, u8* tls, i8 priority, KProcess *parent, const std::shared_ptr<type::KSharedMemory> &tlsMemory) : handle(handle), tid(selfTid), entryPoint(entryPoint), entryArg(entryArg), stackTop(stackTop), tls(tls), priority(priority), parent(parent), ctxMemory(tlsMemory), KSyncObject(state,
        KType::KThread) {
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        Kill();
    }

    void KThread::Start() {
        if (status == Status::Created) {
            if (tid == parent->pid)
                parent->status = KProcess::Status::Started;
            status = Status::Running;

            state.nce->StartThread(entryArg, handle, parent->threads.at(tid));
        }
    }

    void KThread::Kill() {
        if (status != Status::Dead) {
            status = Status::Dead;
            Signal();

            tgkill(parent->pid, tid, SIGTERM);
        }
    }

    void KThread::UpdatePriority(i8 priority) {
        this->priority = priority;
        auto priorityValue{androidPriority.Rescale(switchPriority, priority)};

        if (setpriority(PRIO_PROCESS, static_cast<id_t>(tid), priorityValue) == -1)
            throw exception("Couldn't set process priority to {} for PID: {}", priorityValue, tid);
    }
}
