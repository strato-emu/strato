// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <cxxabi.h>
#include <unistd.h>
#include <common/signal.h>
#include <common/trace.h>
#include <nce.h>
#include <os.h>
#include "KProcess.h"
#include "KThread.h"

namespace skyline::kernel::type {
    KThread::KThread(const DeviceState &state, KHandle handle, KProcess *parent, size_t id, void *entry, u64 argument, void *stackTop, i8 priority, u8 idealCore)
        : handle(handle),
          parent(parent),
          id(id),
          entry(entry),
          entryArgument(argument),
          stackTop(stackTop),
          priority(priority),
          basePriority(priority),
          idealCore(idealCore),
          coreId(idealCore),
          KSyncObject(state, KType::KThread) {
        affinityMask.set(coreId);
    }

    KThread::~KThread() {
        Kill(true);
        if (thread.joinable())
            thread.join();
        if (preemptionTimer)
            timer_delete(preemptionTimer);
    }

    void KThread::StartThread() {
        pthread = pthread_self();
        std::array<char, 16> threadName{};
        if (int result{pthread_getname_np(pthread, threadName.data(), threadName.size())})
            Logger::Warn("Failed to get the thread name: {}", strerror(result));

        if (int result{pthread_setname_np(pthread, fmt::format("HOS-{}", id).c_str())})
            Logger::Warn("Failed to set the thread name: {}", strerror(result));
        Logger::UpdateTag();

        if (!ctx.tpidrroEl0)
            ctx.tpidrroEl0 = parent->AllocateTlsSlot();

        ctx.state = &state;
        state.ctx = &ctx;
        state.thread = shared_from_this();

        if (setjmp(originalCtx)) { // Returns 1 if it's returning from guest, 0 otherwise
            state.scheduler->RemoveThread();

            {
                std::scoped_lock lock{statusMutex};
                running = false;
                ready = false;
                statusCondition.notify_all();
            }

            Signal();

            if (threadName[0] != 'H' || threadName[1] != 'O' || threadName[2] != 'S' || threadName[3] != '-') {
                if (int result{pthread_setname_np(pthread, threadName.data())})
                    Logger::Warn("Failed to set the thread name: {}", strerror(result));
                Logger::UpdateTag();
            }

            return;
        }

        struct sigevent event{
            .sigev_signo = Scheduler::PreemptionSignal,
            .sigev_notify = SIGEV_THREAD_ID,
            .sigev_notify_thread_id = gettid(),
        };
        if (timer_create(CLOCK_THREAD_CPUTIME_ID, &event, &preemptionTimer))
            throw exception("timer_create has failed with '{}'", strerror(errno));

        signal::SetSignalHandler({SIGINT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV}, nce::NCE::SignalHandler);
        signal::SetSignalHandler({Scheduler::YieldSignal, Scheduler::PreemptionSignal}, Scheduler::SignalHandler, false); // We want futexes to fail and their predicates rechecked

        {
            std::scoped_lock lock{statusMutex};
            ready = true;
            statusCondition.notify_all();
        }

        try {
            if (!Scheduler::YieldPending)
                state.scheduler->WaitSchedule();
            while (Scheduler::YieldPending) {
                // If there is a yield pending on us after thread creation
                state.scheduler->Rotate();
                Scheduler::YieldPending = false;
                state.scheduler->WaitSchedule();
            }

            TRACE_EVENT_BEGIN("guest", "Guest");

            asm volatile(
            "MRS X0, TPIDR_EL0\n\t"
            "MSR TPIDR_EL0, %x0\n\t" // Set TLS to ThreadContext
            "STR X0, [%x0, #0x2A0]\n\t" // Write ThreadContext::hostTpidrEl0
            "MOV X0, SP\n\t"
            "STR X0, [%x0, #0x2A8]\n\t" // Write ThreadContext::hostSp
            "MOV SP, %x1\n\t" // Replace SP with guest stack
            "MOV LR, %x2\n\t" // Store entry in Link Register so it's jumped to on return
            "MOV X0, %x3\n\t" // Store the argument in X0
            "MOV X1, %x4\n\t" // Store the thread handle in X1, NCA applications require this
            "MOV X2, XZR\n\t" // Zero out other GP and SIMD registers, not doing this will break applications
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
            "MSR NZCV, XZR\n\t"
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
        } catch (const std::exception &e) {
            Logger::Error(e.what());
            Logger::EmulationContext.Flush();
            if (id) {
                signal::BlockSignal({SIGINT});
                state.process->Kill(false);
            }
            abi::__cxa_end_catch();
            std::longjmp(originalCtx, true);
        } catch (const signal::SignalException &e) {
            if (e.signal != SIGINT) {
                Logger::Error(e.what());
                Logger::EmulationContext.Flush();
                if (id) {
                    signal::BlockSignal({SIGINT});
                    state.process->Kill(false);
                }
            }
            abi::__cxa_end_catch();
            std::longjmp(originalCtx, true);
        }
    }

    void KThread::Start(bool self) {
        std::unique_lock lock(statusMutex);
        if (!running) {
            {
                std::scoped_lock migrationLock{coreMigrationMutex};
                auto thisShared{shared_from_this()};
                coreId = state.scheduler->GetOptimalCoreForThread(thisShared).id;
                state.scheduler->InsertThread(thisShared);
            }

            running = true;
            killed = false;
            statusCondition.notify_all();
            if (self) {
                lock.unlock();
                StartThread();
            } else {
                thread = std::thread(&KThread::StartThread, this);
            }
        }
    }

    void KThread::Kill(bool join) {
        std::unique_lock lock(statusMutex);
        if (!killed && running) {
            statusCondition.wait(lock, [this]() { return ready || killed; });
            if (!killed) {
                pthread_kill(pthread, SIGINT);
                killed = true;
                statusCondition.notify_all();
            }
        }
        if (join)
            statusCondition.wait(lock, [this]() { return !running; });
    }

    void KThread::SendSignal(int signal) {
        std::unique_lock lock(statusMutex);
        statusCondition.wait(lock, [this]() { return ready || killed; });
        if (!killed && running)
            pthread_kill(pthread, signal);
    }

    void KThread::ArmPreemptionTimer(std::chrono::nanoseconds timeToFire) {
        std::unique_lock lock(statusMutex);
        statusCondition.wait(lock, [this]() { return ready || killed; });
        if (!killed && running) {
            struct itimerspec spec{.it_value = {
                .tv_nsec = std::min(static_cast<i64>(timeToFire.count()), constant::NsInSecond),
                .tv_sec = std::max(std::chrono::duration_cast<std::chrono::seconds>(timeToFire).count() - 1, 0LL),
            }};
            timer_settime(preemptionTimer, 0, &spec, nullptr);
            isPreempted = true;
        }
    }

    void KThread::DisarmPreemptionTimer() {
        if (!isPreempted) [[unlikely]]
            return;

        std::unique_lock lock(statusMutex);
        statusCondition.wait(lock, [this]() { return ready || killed; });
        if (!killed && running) {
            struct itimerspec spec{};
            timer_settime(preemptionTimer, 0, &spec, nullptr);
            isPreempted = false;
        }
    }

    void KThread::UpdatePriorityInheritance() {
        std::unique_lock lock{waiterMutex};

        std::shared_ptr<KThread> waitingOn{waitThread};
        i8 currentPriority{priority.load()};
        while (waitingOn) {
            i8 ownerPriority;
            do {
                // Try to CAS the priority of the owner with the current thread
                // If the new priority is equivalent to the current priority then we don't need to CAS
                ownerPriority = waitingOn->priority.load();
                if (ownerPriority <= currentPriority)
                    return;
            } while (!waitingOn->priority.compare_exchange_strong(ownerPriority, currentPriority));

            if (ownerPriority != currentPriority) {
                std::unique_lock waiterLock{waitingOn->waiterMutex, std::try_to_lock};
                if (!waiterLock) {
                    // We want to avoid a deadlock here from the thread holding waitingOn->waiterMutex waiting for waiterMutex
                    // We use a fallback mechanism to avoid this, resetting the state and trying again after being able to successfully acquire waitingOn->waiterMutex once
                    waitingOn->priority = ownerPriority;

                    lock.unlock();

                    waiterLock.lock();
                    waiterLock.unlock();

                    lock.lock();
                    waitingOn = waitThread;

                    continue;
                }

                auto nextThread{waitingOn->waitThread};
                if (nextThread) {
                    // We need to update the location of the owner thread in the waiter queue of the thread it's waiting on
                    std::unique_lock nextWaiterLock{nextThread->waiterMutex, std::try_to_lock};
                    if (!nextWaiterLock) {
                        // We want to avoid a deadlock here from the thread holding nextThread->waiterMutex waiting for waiterMutex or waitingOn->waiterMutex
                        waitingOn->priority = ownerPriority;

                        lock.unlock();
                        waiterLock.unlock();

                        nextWaiterLock.lock();
                        nextWaiterLock.unlock();

                        lock.lock();
                        waitingOn = waitThread;

                        continue;
                    }

                    auto &piWaiters{nextThread->waiters};
                    piWaiters.erase(std::find(piWaiters.begin(), piWaiters.end(), waitingOn));
                    piWaiters.insert(std::upper_bound(piWaiters.begin(), piWaiters.end(), currentPriority, KThread::IsHigherPriority), waitingOn);
                    break;
                }
                state.scheduler->UpdatePriority(waitingOn);
                waitingOn = nextThread;
            } else {
                break;
            }
        }
    }
}
