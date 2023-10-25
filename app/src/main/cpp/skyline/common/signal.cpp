// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <dlfcn.h>
#include <unwind.h>
#include <fcntl.h>
#include <cxxabi.h>
#include "signal.h"

namespace skyline::signal {
    thread_local std::exception_ptr SignalExceptionPtr;

    void ExceptionThrow() {
        std::rethrow_exception(SignalExceptionPtr);
    }

    void SleepTillExit() {
        // We don't want to actually exit the process ourselves as it'll automatically be restarted gracefully due to a timeout after being unable to exit within a fixed duration
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

    using sa_sigaction = void (*)(int, siginfo *, void *);

    /**
     * @brief A signal handler wrapper which restores the previous signal handler for the signal
     */
    struct DefaultSignalHandler {
        ~DefaultSignalHandler();

        sa_sigaction function{};
    };

    static std::array<DefaultSignalHandler, NSIG> DefaultHandlers{}; //!< Signal handlers for signals in host code

    DefaultSignalHandler::~DefaultSignalHandler() {
        if (function) {
            int signal = static_cast<int>(std::distance(DefaultHandlers.data(), this));

            struct sigaction oldAction{};
            Sigaction(signal, nullptr, &oldAction);

            struct sigaction action{
                .sa_sigaction = function,
                .sa_flags = oldAction.sa_flags,
            };
            Sigaction(signal, &action);
        }
    }

    static std::array<GuestSignalAction, NSIG> GuestHandlers{}; //!< Signal handlers for signals in guest code

    /**
     * @brief A signal handler for handling signals coming from guest code
     * @note As this handler might be called from guest code, it needs to restore the host TLS before executing any host code
     * @details Stack protector is disabled as it stores data in TLS at the function epilogue and verifies it at the prolog, we cannot allow writes to guest TLS prior to restoring it
     */
    void __attribute__((no_stack_protector)) GuestSafeSignalHandler(int signum, siginfo *info, ucontext *context) {
        void *tls{}; // The TLS value prior to being restored, if it is
        if (TlsRestorer)
            tls = TlsRestorer();

        // If the TLS was restored, the signal happened in guest code
        if (tls != nullptr) {
            if (auto guestHandler = GuestHandlers[static_cast<size_t>(signum)])
                guestHandler(signum, info, context, &tls);
            else
                LOGWNF("Unhandled guest signal {}, PC: 0x{:x}, Fault address: 0x{:x}", signum, context->uc_mcontext.pc, context->uc_mcontext.fault_address);
        } else {
            if (auto defaultHandler = DefaultHandlers[static_cast<size_t>(signum)].function)
                defaultHandler(signum, info, context);
            else
                LOGWNF("Unhandled host signal {}, PC: 0x{:x}, Fault address: 0x{:x}", signum, context->uc_mcontext.pc, context->uc_mcontext.fault_address);
        }

        if (tls)
            asm volatile("MSR TPIDR_EL0, %x0"::"r"(tls));
    }

    /**
     * @brief Installs the guest safe signal handler for the given signals
     */
    void InstallSignalHandler(std::initializer_list<int> signals, bool syscallRestart) {
        static std::array<std::once_flag, NSIG> once{};

        struct sigaction action{
            .sa_sigaction = reinterpret_cast<sa_sigaction>(GuestSafeSignalHandler),
            .sa_flags = SA_SIGINFO | SA_EXPOSE_TAGBITS | SA_RESTART | SA_ONSTACK,
        };

        for (int signal : signals) {
            std::call_once(once[static_cast<size_t>(signal)], [&] {
                struct sigaction oldAction{};
                Sigaction(signal, &action, &oldAction);

                auto oldFlags = oldAction.sa_flags;
                if (oldFlags) {
                    oldFlags &= ~SA_UNSUPPORTED; // Mask out kernel not supporting old sigaction() bits
                    oldFlags |= SA_SIGINFO | SA_EXPOSE_TAGBITS | SA_RESTART | SA_ONSTACK; // Intentionally ignore these flags for the comparison
                    if (oldFlags != (action.sa_flags | SA_RESTART))
                        throw exception("Old sigaction flags aren't equivalent to the replaced signal: {:#b} | {:#b}", oldFlags, action.sa_flags);
                }

                // Save the old handler for the signal, ignore SIG_IGN and SIG_DFL
                if (oldAction.sa_flags & SA_SIGINFO)
                    DefaultHandlers[static_cast<size_t>(signal)].function = oldAction.sa_sigaction;
                else if (oldAction.sa_handler != SIG_IGN && oldAction.sa_handler != SIG_DFL)
                    DefaultHandlers[static_cast<size_t>(signal)].function = reinterpret_cast<sa_sigaction>(oldAction.sa_handler);
            });
        }
    }

    /**
     * @brief Checks if sigchain is hooking sigaction for the given signal
     * @return True if sigchain is hooking sigaction for the given signal
     */
    static bool IsSigchainHooked(int signum) {
        struct sigaction action{}, rawAction{};
        sigaction(signum, nullptr, &action);
        Sigaction(signum, nullptr, &rawAction);

        return action.sa_sigaction != rawAction.sa_sigaction;
    }

    void SetGuestSignalHandler(std::initializer_list<int> signals, GuestSignalAction function, bool syscallRestart) {
        InstallSignalHandler(signals, syscallRestart);

        for (int signal : signals)
            GuestHandlers[static_cast<size_t>(signal)] = function;
    }

    void SetHostSignalHandler(std::initializer_list<int> signals, SignalAction function, bool syscallRestart) {
        for (int signal : signals) {
            struct sigaction action{
                .sa_sigaction = reinterpret_cast<sa_sigaction>(function),
                .sa_flags = SA_SIGINFO | SA_EXPOSE_TAGBITS | SA_ONSTACK | (syscallRestart ? SA_RESTART : 0),
            };

            // If a guest handler exists for the signal, we need to chain it after it
            // We also need to check that sigchain is not hooking sigaction for the given signal or we'd end up overriding it
            if (GuestHandlers[static_cast<size_t>(signal)] && !IsSigchainHooked(signal))
                DefaultHandlers[static_cast<size_t>(signal)].function = reinterpret_cast<sa_sigaction>(function);
            else
                sigaction(signal, &action, nullptr);
        }
    }

    std::string SignalHandlersSummary() {
        auto resolveHandlerAddress = [](void *addr) -> std::string {
            Dl_info info{};
            if (dladdr(addr, &info) == 0)
                return fmt::format("{} (?)", fmt::ptr(addr));

            // Use the the symbol name if available
            if (info.dli_sname != nullptr) {
                // Try to demangle the symbol name as it may be a C++ symbol, otherwise use as is
                int status{};
                char *demangled{abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status)};
                if (status == 0) {
                    std::string symbol{demangled};
                    free(demangled);
                    // Omit the function arguments
                    return fmt::format("{}", symbol.substr(0, symbol.find_first_of('(')));
                } else {
                    return fmt::format("{}", info.dli_sname);
                }
            }

            // Discard the full path from dli_fname and only keep the filename
            std::string_view filename{info.dli_fname};
            if (auto pos = filename.find_last_of('/'); pos != std::string_view::npos)
                filename = filename.substr(pos + 1);

            // If symbol name is not available use the shared object filename + offset
            return fmt::format("{}+{}", filename, reinterpret_cast<uintptr_t>(addr) - reinterpret_cast<uintptr_t>(info.dli_fbase));
        };

        std::string out = "Signal Handlers:\n";

        for (int signum = 1; signum < NSIG; signum++) {
            // Get the handler via our Sigaction proxy first
            struct sigaction action{};
            Sigaction(signum, nullptr, &action);
            // Ignore default handlers
            if (action.sa_handler == SIG_DFL || action.sa_handler == SIG_IGN)
                continue;
            std::string handler = resolveHandlerAddress(reinterpret_cast<void *>(action.sa_sigaction));

            // Get the handler via glibc sigaction as it might be different because of sigchain
            action = {};
            sigaction(signum, nullptr, &action);
            std::string chainedHandler = resolveHandlerAddress(reinterpret_cast<void *>(action.sa_sigaction));

            out += fmt::format("* Signal: {:2}, Handler: {}", signum, handler);

            // Print the guest handler if set
            if (auto guestHandler = GuestHandlers[static_cast<size_t>(signum)])
                out += fmt::format("\n              Guest Handler: {}", resolveHandlerAddress(reinterpret_cast<void *>(guestHandler)));

            // Print the default handler that is called for host signals
            if (auto defaultHandler = DefaultHandlers[static_cast<size_t>(signum)].function) {
                out += fmt::format("\n              Default Handler: {}", resolveHandlerAddress(reinterpret_cast<void *>(defaultHandler)));
            }

            // If the handler retrieved via sigaction is different to the one retrieved via our proxy, it's been chained by sigchain
            if (handler != chainedHandler)
                out += fmt::format(" -> Chained Handler: {}", chainedHandler);

            out += '\n';
        }

        return out;
    }

    void Sigprocmask(int how, const sigset_t &set, sigset_t *oldSet) {
        static decltype(&pthread_sigmask) real{};
        if (!real)
            real = GetLibcFunction<decltype(&sigprocmask)>("sigprocmask");
        if (real(how, &set, oldSet) < 0)
            throw exception("sigprocmask has failed with {}", strerror(errno));
    }
}
