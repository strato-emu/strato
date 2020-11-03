// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <android/log.h>
#include <common/signal.h>
#include <nce.h>
#include <os.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument, void *stackTop, i8 priority, i8 idealCore) : handle(handle), parent(parent), id(id), entry(entry), entryArgument(argument), stackTop(stackTop), idealCore(idealCore), coreId(idealCore), KSyncObject(state, KType::KThread) {
        affinityMask.set(coreId);
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        if (running && pthread != pthread_self()) {
            pthread_kill(pthread, SIGINT);
            if (thread)
                thread->join();
            else
                pthread_join(pthread, nullptr);
        }
    }

    void KThread::StartThread() {
        std::array<char, 16> threadName;
        pthread_getname_np(pthread, threadName.data(), threadName.size());
        pthread_setname_np(pthread, fmt::format("HOS-{}", id).c_str());
        state.logger->UpdateTag();

        if (!ctx.tpidrroEl0)
            ctx.tpidrroEl0 = parent->AllocateTlsSlot();

        ctx.state = &state;
        state.ctx = &ctx;
        state.thread = shared_from_this();

        if (setjmp(originalCtx)) { // Returns 1 if it's returning from guest, 0 otherwise
            running = false;
            Signal();

            pthread_setname_np(pthread, threadName.data());
            state.logger->UpdateTag();

            return;
        }

        signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, nce::NCE::SignalHandler);

        asm volatile(
        "MRS X0, TPIDR_EL0\n\t"
        "MSR TPIDR_EL0, %x0\n\t" // Set TLS to ThreadContext
        "STR X0, [%x0, #0x2A0]\n\t" // Write ThreadContext::hostTpidrEl0
        "MOV X0, SP\n\t"
        "STR X0, [%x0, #0x2A8]\n\t" // Write ThreadContext::hostSp
        "MOV SP, %x1\n\t" // Replace SP with guest stack
        "MOV LR, %x2\n\t" // Store entry in Link Register so it is jumped to on return
        "MOV X0, %x3\n\t" // Store the argument in X0
        "MOV X1, %x4\n\t" // Store the thread handle in X1, NCA applications require this
        "MOV X2, XZR\n\t"
        "MOV X3, XZR\n\t"
        "MOV X4, XZR\n\t"
        "MOV X5, XZR\n\t"
        "MOV X6, XZR\n\t"
        "MOV X7, XZR\n\t"
        "MOV X8, XZR\n\t"
        "MOV X9, XZR\n\t"
        "MOV X10, XZR\n\t"
        "MOV X11, XZR\n\t"
        "MOV X12, XZR\n\t"
        "MOV X13, XZR\n\t"
        "MOV X14, XZR\n\t"
        "MOV X15, XZR\n\t"
        "MOV X16, XZR\n\t"
        "MOV X17, XZR\n\t"
        "MOV X18, XZR\n\t"
        "MOV X19, XZR\n\t"
        "MOV X20, XZR\n\t"
        "MOV X21, XZR\n\t"
        "MOV X22, XZR\n\t"
        "MOV X23, XZR\n\t"
        "MOV X24, XZR\n\t"
        "MOV X25, XZR\n\t"
        "MOV X26, XZR\n\t"
        "MOV X27, XZR\n\t"
        "MOV X28, XZR\n\t"
        "MOV X29, XZR\n\t"
        "MSR FPSR, XZR\n\t"
        "MSR FPCR, XZR\n\t"
        "DUP V0.16B, WZR\n\t"
        "DUP V1.16B, WZR\n\t"
        "DUP V2.16B, WZR\n\t"
        "DUP V3.16B, WZR\n\t"
        "DUP V4.16B, WZR\n\t"
        "DUP V5.16B, WZR\n\t"
        "DUP V6.16B, WZR\n\t"
        "DUP V7.16B, WZR\n\t"
        "DUP V8.16B, WZR\n\t"
        "DUP V9.16B, WZR\n\t"
        "DUP V10.16B, WZR\n\t"
        "DUP V11.16B, WZR\n\t"
        "DUP V12.16B, WZR\n\t"
        "DUP V13.16B, WZR\n\t"
        "DUP V14.16B, WZR\n\t"
        "DUP V15.16B, WZR\n\t"
        "DUP V16.16B, WZR\n\t"
        "DUP V17.16B, WZR\n\t"
        "DUP V18.16B, WZR\n\t"
        "DUP V19.16B, WZR\n\t"
        "DUP V20.16B, WZR\n\t"
        "DUP V21.16B, WZR\n\t"
        "DUP V22.16B, WZR\n\t"
        "DUP V23.16B, WZR\n\t"
        "DUP V24.16B, WZR\n\t"
        "DUP V25.16B, WZR\n\t"
        "DUP V26.16B, WZR\n\t"
        "DUP V27.16B, WZR\n\t"
        "DUP V28.16B, WZR\n\t"
        "DUP V29.16B, WZR\n\t"
        "DUP V30.16B, WZR\n\t"
        "DUP V31.16B, WZR\n\t"
        "RET"
        :
        : "r"(&ctx), "r"(stackTop), "r"(entry), "r"(entryArgument), "r"(handle)
        : "x0", "x1", "lr"
        );

        __builtin_unreachable();
    }

    void KThread::Start(bool self) {
        std::unique_lock lock(mutex);
        if (!running) {
            running = true;
            state.logger->Debug("Starting thread #{}", id);
            if (self) {
                pthread = pthread_self();
                lock.unlock();
                StartThread();
            } else {
                thread.emplace(&KThread::StartThread, this);
                pthread = thread->native_handle();
            }
        }
    }

    void KThread::Kill(bool join) {
        std::lock_guard lock(mutex);
        if (running) {
            pthread_kill(pthread, SIGINT);
            if (join)
                pthread_join(pthread, nullptr);
            running = false;
        }
    }

    void KThread::UpdatePriority(i8 priority) {
        this->priority = priority;
    }
}
