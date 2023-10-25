// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::signal {
    /**
     * @brief The structure of a stack frame entry in the ARMv8 ABI
     */
    struct StackFrame {
        StackFrame *next;
        void *lr;
    };

    /**
     * @brief A scoped way to block a stack trace beyond the scope of this object
     * @note This is used for JNI functions where the stack trace will be determined as they often contain invalid stack frames which'd cause a SIGSEGV
     */
    struct ScopedStackBlocker {
        StackFrame realFrame;

        __attribute__((noinline)) ScopedStackBlocker() {
            StackFrame *frame;
            asm("MOV %0, FP" : "=r"(frame));
            realFrame = *frame;
            frame->next = nullptr;
            frame->lr = nullptr;
        }

        __attribute__((noinline)) ~ScopedStackBlocker() {
            StackFrame *frame;
            asm("MOV %0, FP" : "=r"(frame));
            frame->next = realFrame.next;
            frame->lr = realFrame.lr;
        }
    };

    /**
     * @brief An exception object that is designed specifically to hold Linux signals
     * @note This doesn't inherit std::exception as it shouldn't be caught as such
     * @note Refer to the manpage siginfo(3) for information on members
     */
    class SignalException {
      public:
        int signal{};
        void *pc{};
        void *fault{};
        std::vector<void *> frames; //!< A vector of all stack frame entries prior to the signal occuring

        std::string what() const {
            if (!fault)
                return fmt::format("Signal: {} (PC: 0x{:X})", strsignal(signal), reinterpret_cast<uintptr_t>(pc));
            else
                return fmt::format("Signal: {} @ 0x{:X} (PC: 0x{:X})", strsignal(signal), reinterpret_cast<uintptr_t>(fault), reinterpret_cast<uintptr_t>(pc));
        }
    };

    /**
     * @brief A signal handler which automatically throws an exception with the corresponding signal metadata in a SignalException
     * @note A termination handler is set in this which prevents any termination from going through as to break out of 'noexcept', do not use std::terminate in a catch clause for this exception
     */
    void ExceptionalSignalHandler(int signal, siginfo *, ucontext *context);

    /**
     * @brief Our delegator for sigaction, we need to do this due to sigchain hooking bionic's sigaction and it intercepting signals before they're passed onto userspace
     * This not only leads to performance degradation but also requires host TLS to be in the TLS register which we cannot ensure for in-guest signals
     */
    void Sigaction(int signal, const struct sigaction *action, struct sigaction *oldAction = nullptr);

    /**
     * @brief If the TLS value of the code running prior to a signal has a custom TLS value, this should be used to restore it
     * @param function A function which is inert if the TLS isn't required to be restored, it should return nullptr if TLS wasn't restored else the old TLS value
     */
    void SetTlsRestorer(void *(*function)());

    /**
     * @brief Signature of a sa_sigaction callback
     */
    using SignalAction = void (*)(int signum, siginfo *info, ucontext *context);

    /**
     * @brief Signature of a guest signal handler
     * @note The 4th argument points to the guest TLS, set it to 'nullptr' to avoid restoring the guest TLS and keep the host TLS (e.g. long jumps to host code)
     */
    using GuestSignalAction = void (*)(int signum, siginfo *info, ucontext *context, void **tls);

    /**
     * @brief Sets signal handlers for the given signals coming from guest code
     * @param function A sa_sigaction callback with a pointer to the guest TLS as the 4th argument
     * @param syscallRestart If a system call running during the signal will be seamlessly restarted or return an error (Corresponds to SA_RESTART)
     */
    void SetGuestSignalHandler(std::initializer_list<int> signals, GuestSignalAction function, bool syscallRestart = true);

    /**
     * @brief Sets signal handlers for the given signals coming from host code
     * @param function A sa_sigaction callback
     * @param syscallRestart If a system call running during the signal will be seamlessly restarted or return an error (Corresponds to SA_RESTART)
     */
    void SetHostSignalHandler(std::initializer_list<int> signals, SignalAction function, bool syscallRestart = true);

    /**
     * @brief Returns a human-readable string containing information about signal handlers currently registered for all signals that aren't default handled or ignored
     */
    std::string PrintSignalHandlers();

    /**
     * @brief Our delegator for sigprocmask, required due to libsigchain hooking this
     */
    void Sigprocmask(int how, const sigset_t &set, sigset_t *oldSet = nullptr);

    inline void BlockSignal(std::initializer_list<int> signals) {
        sigset_t set{};
        for (int signal : signals)
            sigaddset(&set, signal);
        Sigprocmask(SIG_BLOCK, set, nullptr);
    }
}
