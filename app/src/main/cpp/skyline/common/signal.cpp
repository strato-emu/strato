// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <unistd.h>
#include <dlfcn.h>
#include <unwind.h>
#include <android/log.h>
#include "signal.h"

namespace skyline::signal {
    thread_local SignalException signalException;

    void ExceptionThrow() {
        throw signalException;
    }

    void ExceptionalSignalHandler(int signal, siginfo *info, ucontext *context) {
        signalException.signal = signal;
        signalException.pc = context->uc_mcontext.pc;
        if (signal == SIGSEGV)
            signalException.faultAddress = info->si_addr;
        context->uc_mcontext.pc = reinterpret_cast<u64>(&ExceptionThrow);
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

    std::array<void (*)(int, struct siginfo *, void *), NSIG> DefaultSignalHandlers;

    struct ThreadSignalHandler {
        pthread_key_t key;
        std::atomic<u32> count;

        void Decrement();

        static void DecrementStatic(ThreadSignalHandler *thiz) {
            thiz->Decrement();
        }
    };

    std::array<ThreadSignalHandler, NSIG> ThreadSignalHandlers;

    void ThreadSignalHandler::Decrement() {
        u32 current;
        while ((current = count.load()) && !count.compare_exchange_strong(current, --current));
        if (current == 0) {
            int signal{static_cast<int>(this - ThreadSignalHandlers.data())};

            struct sigaction oldAction;
            Sigaction(signal, nullptr, &oldAction);

            struct sigaction action{
                .sa_sigaction = DefaultSignalHandlers.at(signal),
                .sa_flags = oldAction.sa_flags,
            };
            Sigaction(signal, &action);
        }
    }

    void ThreadSignalHandler(int signal, siginfo *info, ucontext *context) {
        void *tls{}; // The TLS value prior to being restored if it is
        if (TlsRestorer)
            tls = TlsRestorer();

        auto handler{reinterpret_cast<void (*)(int, struct siginfo *, ucontext *, void *)>(pthread_getspecific(ThreadSignalHandlers.at(signal).key))};
        if (handler) {
            handler(signal, info, context, tls);
        } else {
            auto defaultHandler{DefaultSignalHandlers.at(signal)};
            if (defaultHandler)
                defaultHandler(signal, info, context);
        }

        if (tls)
            asm volatile("MSR TPIDR_EL0, %x0"::"r"(tls));
    }

    void SetSignalHandler(std::initializer_list<int> signals, void (*function)(int, struct siginfo *, ucontext *, void *)) {
        static std::array<std::once_flag, NSIG> signalHandlerOnce{};

        stack_t stack;
        sigaltstack(nullptr, &stack);
        struct sigaction action{
            .sa_sigaction = reinterpret_cast<void (*)(int, siginfo *, void *)>(ThreadSignalHandler),
            .sa_flags = SA_RESTART | SA_SIGINFO | (stack.ss_sp && stack.ss_size ? SA_ONSTACK : 0),
        };

        for (int signal : signals) {
            auto &threadHandler{ThreadSignalHandlers.at(signal)};
            std::call_once(signalHandlerOnce[signal], [signal, action, &threadHandler]() {
                if (int result = pthread_key_create(&threadHandler.key, reinterpret_cast<void (*)(void *)>(&ThreadSignalHandler::DecrementStatic)))
                    throw exception("Failed to create per-thread signal handler pthread key: {}", strerror(result));

                struct sigaction oldAction;
                Sigaction(signal, &action, &oldAction);
                if (oldAction.sa_flags && oldAction.sa_flags != action.sa_flags)
                    throw exception("Old sigaction flags aren't equivalent to the replaced signal: {:#b} | {:#b}", oldAction.sa_flags, action.sa_flags);

                DefaultSignalHandlers.at(signal) = oldAction.sa_sigaction;
            });
            if (!pthread_getspecific(ThreadSignalHandlers.at(signal).key))
                threadHandler.count++;
            pthread_setspecific(ThreadSignalHandlers.at(signal).key, reinterpret_cast<void *>(function));
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
