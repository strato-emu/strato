// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <nce.h>
#include <os.h>
#include <android/log.h>
#include <dlfcn.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument, void *stackTop, i8 priority) : handle(handle), parent(parent), id(id), entry(entry), entryArgument(argument), stackTop(stackTop), KSyncObject(state, KType::KThread) {
        UpdatePriority(priority);
    }

    KThread::~KThread() {
        Kill();
    }

    /**
     * @brief Our delegator for sigaction, we need to do this due to sigchain hooking bionic's sigaction and it intercepting signals before they're passed onto userspace
     * This not only leads to performance degradation but also requires host TLS to be in the TLS register which we cannot ensure for in-guest signals
     */
    inline void Sigaction(int signal, const struct sigaction &action, struct sigaction *oldAction = nullptr) {
        static decltype(&sigaction) realSigaction{};
        if (!realSigaction) {
            void *libc{dlopen("libc.so", RTLD_LOCAL | RTLD_LAZY)};
            if (!libc)
                throw exception("dlopen-ing libc has failed with: {}", dlerror());
            realSigaction = reinterpret_cast<decltype(&sigaction)>(dlsym(libc, "sigaction"));
            if (!realSigaction)
                throw exception("Cannot find 'sigaction' in libc: {}", dlerror());
        }
        if (realSigaction(signal, &action, oldAction) < 0)
            throw exception("sigaction has failed with {}", strerror(errno));
    }

    void KThread::StartThread() {
        pthread_setname_np(pthread_self(), fmt::format("HOS-{}", id).c_str());

        if (!ctx.tpidrroEl0)
            ctx.tpidrroEl0 = parent->AllocateTlsSlot();

        ctx.state = &state;
        state.ctx = &ctx;
        state.thread = shared_from_this();

        struct sigaction sigact{
            .sa_sigaction = &nce::NCE::SignalHandler,
            .sa_flags = SA_SIGINFO,
        };
        for (int signal : {SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV})
            Sigaction(signal, sigact);

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
        if (!running) {
            running = true;
            state.logger->Debug("Starting thread #{}", id);
            if (self)
                StartThread();
            else
                thread.emplace(&KThread::StartThread, this);
        }
    }

    void KThread::Kill() {
        if (running) {
            running = false;
            Signal();
        }
    }

    void KThread::UpdatePriority(i8 priority) {
        this->priority = priority;
        auto priorityValue{constant::AndroidPriority.Rescale(constant::HosPriority, priority)};

        if (setpriority(PRIO_PROCESS, getpid(), priorityValue) == -1)
            throw exception("Couldn't set thread priority to {} for #{}", priorityValue, id);
    }
}
