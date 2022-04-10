// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <dlfcn.h>
#include <unwind.h>
#include <fcntl.h>
#include "signal.h"

namespace skyline::signal {
    thread_local std::exception_ptr SignalExceptionPtr;

    void ExceptionThrow() {
        std::rethrow_exception(SignalExceptionPtr);
    }

    void SleepTillExit() {
        // We don't want to actually exit the process ourselves as it'll automatically be restarted gracefully due to a timeout after being unable to exit within a fixed duration
        Logger::EmulationContext.TryFlush(); // We want to attempt to flush logs before exiting
        while (true)
            sleep(std::numeric_limits<int>::max()); // We just trigger the timeout wait by sleeping forever
    }

    inline StackFrame *SafeFrameRecurse(size_t depth, StackFrame *frame) {
        if (frame) {
            for (size_t it{}; it < depth; it++) {
                if (frame->lr && frame->next)
                    frame = frame->next;
                else
                    SleepTillExit();
            }
        } else {
            SleepTillExit();
        }
        return frame;
    }

    void TerminateHandler() {
        auto exception{std::current_exception()};
        if (exception && exception == SignalExceptionPtr) {
            StackFrame *frame;
            asm("MOV %0, FP" : "=r"(frame));
            frame = SafeFrameRecurse(2, frame); // We unroll past 'std::terminate'

            static void *exceptionThrowEnd{};
            if (!exceptionThrowEnd) {
                // We need to find the function bounds for ExceptionThrow, if we haven't already
                u32 *it{reinterpret_cast<u32 *>(&ExceptionThrow) + 1};
                while (_Unwind_FindEnclosingFunction(it) == &ExceptionThrow)
                    it++;
                exceptionThrowEnd = it - 1;
            }

            auto lookupFrame{frame};
            bool hasAdvanced{};
            while (lookupFrame && lookupFrame->lr) {
                if (lookupFrame->lr >= reinterpret_cast<void *>(&ExceptionThrow) && lookupFrame->lr < exceptionThrowEnd) {
                    // We need to check if the current stack frame is from ExceptionThrow
                    // As we need to skip past it (2 frames) and be able to recognize when we're in an infinite loop
                    if (!hasAdvanced) {
                        frame = SafeFrameRecurse(2, lookupFrame);
                        hasAdvanced = true;
                    } else {
                        SleepTillExit(); // We presumably have no exception handlers left on the stack to consume the exception, it's time to quit
                    }
                }
                lookupFrame = lookupFrame->next;
            }

            if (!frame->next)
                SleepTillExit(); // We don't know the frame's stack boundaries, the only option is to quit

            asm("MOV SP, %x0\n\t" // Stack frame is the first item on a function's stack, it's used to calculate calling function's stack pointer
                "MOV LR, %x1\n\t"
                "MOV FP, %x2\n\t" // The stack frame of the calling function should be set
                "BR %x3"
            : : "r"(frame + 1), "r"(frame->lr), "r"(frame->next), "r"(&ExceptionThrow));

            __builtin_unreachable();
        } else {
            SleepTillExit(); // We don't want to delegate to the older terminate handler as it might cause an exit
        }
    }

    void ExceptionalSignalHandler(int signal, siginfo *info, ucontext *context) {
        SignalException signalException;
        signalException.signal = signal;
        signalException.pc = reinterpret_cast<void *>(context->uc_mcontext.pc);
        if (signal == SIGSEGV)
            signalException.fault = info->si_addr;

        signalException.frames.push_back(reinterpret_cast<void *>(context->uc_mcontext.pc));
        StackFrame *frame{reinterpret_cast<StackFrame *>(context->uc_mcontext.regs[29])};
        while (frame && frame->lr) {
            signalException.frames.push_back(frame->lr);
            frame = frame->next;
        }

        SignalExceptionPtr = std::make_exception_ptr(signalException);
        context->uc_mcontext.pc = reinterpret_cast<u64>(&ExceptionThrow);

        std::set_terminate(TerminateHandler);

        Logger::EmulationContext.TryFlush(); // We want to attempt to flush all logs in case exception handling fails and infloops
    }

    template<typename Signature>
    Signature GetLibcFunction(const char *symbol) {
        void *libc{dlopen("libc.so", RTLD_LOCAL | RTLD_LAZY)};
        if (!libc)
            throw exception("dlopen-ing libc has failed with: {}", dlerror());
        auto function{reinterpret_cast<Signature>(dlsym(libc, symbol))};
        if (!function)
            throw exception("Cannot find '{}' in libc: {}", symbol, dlerror());
        return function;
    }

    void Sigaction(int signal, const struct sigaction *action, struct sigaction *oldAction) {
        static decltype(&sigaction) real{};
        if (!real)
            real = GetLibcFunction<decltype(&sigaction)>("sigaction");
        if (real(signal, action, oldAction) < 0)
            throw exception("sigaction has failed with {}", strerror(errno));
    }

    static void *(*TlsRestorer)(){};

    void SetTlsRestorer(void *(*function)()) {
        TlsRestorer = function;
    }

    struct DefaultSignalHandler {
        void (*function)(int, struct siginfo *, void *){};

        ~DefaultSignalHandler();
    };

    std::array<DefaultSignalHandler, NSIG> DefaultSignalHandlers;

    DefaultSignalHandler::~DefaultSignalHandler() {
        if (function) {
            int signal{static_cast<int>(this - DefaultSignalHandlers.data())};

            struct sigaction oldAction;
            Sigaction(signal, nullptr, &oldAction);

            struct sigaction action{
                .sa_sigaction = function,
                .sa_flags = oldAction.sa_flags,
            };
            Sigaction(signal, &action);
        }
    }

    thread_local std::array<SignalHandler, NSIG> ThreadSignalHandlers{};

    __attribute__((no_stack_protector)) // Stack protector stores data in TLS at the function epilogue and verifies it at the prolog, we cannot allow writes to guest TLS and may switch to an alternative TLS during the signal handler and have disabled the stack protector as a result
    void ThreadSignalHandler(int signal, siginfo *info, ucontext *context) {
        void *tls{}; // The TLS value prior to being restored if it is
        if (TlsRestorer)
            tls = TlsRestorer();

        auto handler{ThreadSignalHandlers.at(static_cast<size_t>(signal))};
        if (handler) {
            handler(signal, info, context, &tls);
        } else {
            auto defaultHandler{DefaultSignalHandlers.at(static_cast<size_t>(signal)).function};
            if (defaultHandler)
                defaultHandler(signal, info, context);
        }

        if (tls)
            asm volatile("MSR TPIDR_EL0, %x0"::"r"(tls));
    }

    void SetSignalHandler(std::initializer_list<int> signals, SignalHandler function, bool syscallRestart) {
        static std::array<std::once_flag, NSIG> signalHandlerOnce{};

        struct sigaction action{
            .sa_sigaction = reinterpret_cast<void (*)(int, siginfo *, void *)>(ThreadSignalHandler),
            .sa_flags = SA_SIGINFO | SA_EXPOSE_TAGBITS | (syscallRestart ? SA_RESTART : 0) | SA_ONSTACK,
        };

        for (int signal : signals) {
            std::call_once(signalHandlerOnce[static_cast<size_t>(signal)], [signal, &action]() {
                struct sigaction oldAction;
                Sigaction(signal, &action, &oldAction);
                if (oldAction.sa_flags) {
                    oldAction.sa_flags &= ~SA_UNSUPPORTED; // Mask out kernel not supporting old sigaction() bits
                    oldAction.sa_flags |= SA_SIGINFO | SA_EXPOSE_TAGBITS | SA_RESTART | SA_ONSTACK; // Intentionally ignore these flags for the comparison
                    if (oldAction.sa_flags != (action.sa_flags | SA_RESTART))
                        throw exception("Old sigaction flags aren't equivalent to the replaced signal: {:#b} | {:#b}", oldAction.sa_flags, action.sa_flags);
                }

                DefaultSignalHandlers.at(static_cast<size_t>(signal)).function = (oldAction.sa_flags & SA_SIGINFO) ? oldAction.sa_sigaction : reinterpret_cast<void (*)(int, struct siginfo *, void *)>(oldAction.sa_handler);
            });
            ThreadSignalHandlers.at(static_cast<size_t>(signal)) = function;
        }
    }

    void Sigprocmask(int how, const sigset_t &set, sigset_t *oldSet) {
        static decltype(&pthread_sigmask) real{};
        if (!real)
            real = GetLibcFunction<decltype(&sigprocmask)>("sigprocmask");
        if (real(how, &set, oldSet) < 0)
            throw exception("sigprocmask has failed with {}", strerror(errno));
    }
}
