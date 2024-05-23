// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <jvm.h>
#include <common/trace.h>
#include <kernel/results.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KProcess::TlsPage::TlsPage(u8 *memory) : memory(memory) {}

    u8 *KProcess::TlsPage::ReserveSlot() {
        if (index == constant::TlsSlots)
            return nullptr;
        return memory + (constant::TlsSlotSize * index++);
    }

    KProcess::KProcess(const DeviceState &state) : memory(state), KSyncObject(state, KType::KProcess) {
        trap.InstallStaticInstance();
    }

    KProcess::~KProcess() {
        std::scoped_lock guard{threadMutex};
        disableThreadCreation = true;
        for (const auto &thread : threads)
            thread->Kill(true);
    }

    void KProcess::Kill(bool join, bool all, bool disableCreation) {
        LOGW("Killing {}{}KProcess{}", join ? "and joining " : "", all ? "all threads in " : "HOS-1 in ", disableCreation ? " with new thread creation disabled" : "");
        // disableCreation is set only when gracefully exiting, it being false means an exception/crash occurred
        if (!disableCreation)
            state.jvm->reportCrash();

        bool expected{false};
        if (!join && !alreadyKilled.compare_exchange_strong(expected, true))
            // If the process has already been killed and we don't want to wait for it to join then just instantly return rather than waiting on the mutex
            return;
        else
            alreadyKilled.store(true);

        std::scoped_lock guard{threadMutex};
        if (disableCreation)
            disableThreadCreation = true;
        if (all) {
            for (const auto &thread : threads)
                thread->Kill(join);
        } else if (!threads.empty()) {
            threads[0]->Kill(join);
        }
    }

    void KProcess::InitializeHeapTls() {
        constexpr size_t DefaultHeapSize{0x200000};
        memory.MapHeapMemory(span<u8>{state.process->memory.heap.guest.data(), DefaultHeapSize});
        memory.processHeapSize = DefaultHeapSize;
        tlsExceptionContext = AllocateTlsSlot();
    }

    u8 *KProcess::AllocateTlsSlot() {
        std::scoped_lock lock{tlsMutex};
        u8 *slot;
        for (auto &tlsPage : tlsPages)
            if ((slot = tlsPage->ReserveSlot()))
                return slot;

        bool isAllocated{};

        u8 *pageCandidate{state.process->memory.tlsIo.guest.data()};
        while (state.process->memory.tlsIo.guest.contains(span<u8>(pageCandidate, constant::PageSize))) {
            auto chunk = memory.GetChunk(pageCandidate);
            if (!chunk)
                break;

            if (chunk->second.state == memory::states::Unmapped) {
                memory.MapThreadLocalMemory(span<u8>{pageCandidate, constant::PageSize});
                isAllocated = true;
                break;
            } else {
                pageCandidate = chunk->first + chunk->second.size;
            }
        }

        if (!isAllocated)
            throw exception("Failed to find free memory for a tls slot!");

        // Translate the page address to the host address space so that it can be accessed directly
        pageCandidate = memory.TranslateVirtualPointer<u8 *>(reinterpret_cast<u64>(pageCandidate));

        auto tlsPage{std::make_shared<TlsPage>(pageCandidate)};
        tlsPages.push_back(tlsPage);
        return tlsPage->ReserveSlot();
    }

    std::shared_ptr<KThread> KProcess::CreateThread(void *entry, u64 argument, void *stackTop, std::optional<i8> priority, std::optional<u8> idealCore) {
        std::scoped_lock guard{threadMutex};
        if (disableThreadCreation)
            return nullptr;
        if (!stackTop && threads.empty()) { //!< Main thread stack is created by the kernel and owned by the process
            bool isAllocated{};

            u8 *pageCandidate{memory.stack.guest.data()};
            while (state.process->memory.stack.guest.contains(span<u8>(pageCandidate, state.process->npdm.meta.mainThreadStackSize))) {
                auto chunk{memory.GetChunk(pageCandidate)};
                if (!chunk)
                    break;

                if (chunk->second.state == memory::states::Unmapped && chunk->second.size >= state.process->npdm.meta.mainThreadStackSize) {
                    memory.MapStackMemory(span<u8>{pageCandidate, state.process->npdm.meta.mainThreadStackSize});
                    isAllocated = true;
                    break;
                } else {
                    pageCandidate = chunk->first + chunk->second.size;
                }
            }

            if (!isAllocated)
                throw exception("Failed to map main thread stack!");

            stackTop = pageCandidate + state.process->npdm.meta.mainThreadStackSize;
            mainThreadStack = span<u8>(pageCandidate, state.process->npdm.meta.mainThreadStackSize);
        }
        size_t tid{threads.size() + 1}; //!< The first thread is HOS-1 rather than HOS-0, this is to match the HOS kernel's behaviour

        auto thread{[&]() -> std::shared_ptr<KThread> {
            if (is64bit())
                return NewHandle<KNceThread>(std::ref(*this), tid, entry, argument, stackTop, priority ? *priority : state.process->npdm.meta.mainThreadPriority, idealCore ? *idealCore : state.process->npdm.meta.idealCore).item;
            else
                return NewHandle<KJit32Thread>(std::ref(*this), tid, entry, argument, stackTop, priority ? *priority : state.process->npdm.meta.mainThreadPriority, idealCore ? *idealCore : state.process->npdm.meta.idealCore).item;
        }()};
        threads.push_back(thread);
        return thread;
    }

    void KProcess::ClearHandleTable() {
        std::shared_lock lock(handleMutex);
        handles.clear();
    }

    constexpr u32 HandleWaitersBit{1UL << 30}; //!< A bit which denotes if a mutex psuedo-handle has waiters or not

    Result KProcess::MutexLock(const std::shared_ptr<KThread> &thread, u32 *mutex, KHandle ownerHandle, KHandle tag, bool failOnOutdated) {
        TRACE_EVENT_FMT("kernel", "MutexLock {} @ 0x{:X}", fmt::ptr(mutex), thread->id);

        std::shared_ptr<KThread> owner;
        try {
            owner = GetHandle<KThread>(ownerHandle);
        } catch (const std::out_of_range &) {
            if (__atomic_load_n(mutex, __ATOMIC_SEQ_CST) != (ownerHandle | HandleWaitersBit))
                return failOnOutdated ? result::InvalidCurrentMemory : Result{};

            return result::InvalidHandle;
        }

        bool isHighestPriority;
        {
            std::scoped_lock lock{owner->waiterMutex, thread->waiterMutex}; // We need to lock both mutexes at the same time as we mutate the owner and the current thread, the ordering of locks **must** match MutexUnlock to avoid deadlocks

            u32 value{__atomic_load_n(mutex, __ATOMIC_SEQ_CST)};
            if (value != (ownerHandle | HandleWaitersBit))
                // We ensure that the mutex's value is the handle with the waiter bit set
                return failOnOutdated ? result::InvalidCurrentMemory : Result{};

            auto &waiters{owner->waiters};
            isHighestPriority = waiters.insert(std::upper_bound(waiters.begin(), waiters.end(), thread->priority.load(), KThread::IsHigherPriority), thread) == waiters.begin();
            if (thread == state.thread)
                state.scheduler->RemoveThread();

            thread->waitThread = owner;
            thread->waitMutex = mutex;
            thread->waitTag = tag;
        }

        if (isHighestPriority)
            // If we were the highest priority thread then we need to inherit priorities for all threads we're waiting on recursively
            thread->UpdatePriorityInheritance();

        if (thread == state.thread)
            state.scheduler->WaitSchedule();

        return {};
    }

    void KProcess::MutexUnlock(u32 *mutex) {
        TRACE_EVENT_FMT("kernel", "MutexUnlock {}", fmt::ptr(mutex));

        std::scoped_lock lock{state.thread->waiterMutex};
        auto &waiters{state.thread->waiters};
        auto nextOwnerIt{std::find_if(waiters.begin(), waiters.end(), [mutex](const std::shared_ptr<KThread> &thread) { return thread->waitMutex == mutex; })};
        if (nextOwnerIt != waiters.end()) {
            auto nextOwner{*nextOwnerIt};
            std::scoped_lock nextLock{nextOwner->waiterMutex};
            nextOwner->waitThread = std::shared_ptr<KThread>{nullptr};
            nextOwner->waitMutex = nullptr;

            // Move all threads waiting on this key to the next owner's waiter list
            std::shared_ptr<KThread> nextWaiter{};
            for (auto it{waiters.erase(nextOwnerIt)}, nextIt{std::next(it)}; it != waiters.end(); it = nextIt++) {
                auto thread{*it};
                if (thread->waitMutex == mutex) {
                    nextOwner->waiters.splice(std::upper_bound(nextOwner->waiters.begin(), nextOwner->waiters.end(), (*it)->priority.load(), KThread::IsHigherPriority), waiters, it);
                    thread->waitThread = nextOwner;
                    if (!nextWaiter)
                        nextWaiter = thread;
                }
            }

            if (!waiters.empty()) {
                // If there are threads still waiting on us then try to inherit their priority
                auto highestPriorityThread{waiters.front()};
                i8 newPriority, currentPriority{state.thread->priority.load()};
                do {
                    newPriority = std::min(currentPriority, highestPriorityThread->priority.load());
                } while (currentPriority != newPriority && !state.thread->priority.compare_exchange_strong(currentPriority, newPriority));
                state.scheduler->UpdatePriority(state.thread);
            } else {
                i8 priority, basePriority;
                do {
                    basePriority = state.thread->basePriority.load();
                    priority = state.thread->priority.load();
                } while (priority != basePriority && !state.thread->priority.compare_exchange_strong(priority, basePriority));
                if (priority != basePriority)
                    state.scheduler->UpdatePriority(state.thread);
            }

            if (nextWaiter) {
                // If there is a waiter on the new owner then try to inherit its priority
                i8 priority, ownerPriority;
                do {
                    ownerPriority = nextOwner->priority.load();
                    priority = std::min(ownerPriority, nextWaiter->priority.load());
                } while (ownerPriority != priority && !nextOwner->priority.compare_exchange_strong(ownerPriority, priority));

                __atomic_store_n(mutex, nextOwner->waitTag | HandleWaitersBit, __ATOMIC_SEQ_CST);
            } else {
                __atomic_store_n(mutex, nextOwner->waitTag, __ATOMIC_SEQ_CST);
            }

            // Finally, schedule the next owner accordingly
            state.scheduler->InsertThread(nextOwner);
        } else {
            __atomic_store_n(mutex, 0, __ATOMIC_SEQ_CST);
        }
    }

    Result KProcess::ConditionVariableWait(u32 *key, u32 *mutex, KHandle tag, i64 timeout) {
        TRACE_EVENT_FMT("kernel", "ConditionVariableWait {} ({})", fmt::ptr(key), fmt::ptr(mutex));

        {
            // Update all waiter information
            std::unique_lock lock{state.thread->waiterMutex};
            state.thread->waitThread = std::shared_ptr<KThread>{nullptr};
            state.thread->waitMutex = mutex;
            state.thread->waitTag = tag;
            state.thread->waitConditionVariable = key;
            state.thread->waitSignalled = false;
            state.thread->waitResult = {};
        }

        {
            std::scoped_lock lock{syncWaiterMutex};
            auto queue{syncWaiters.equal_range(key)};
            syncWaiters.insert(std::upper_bound(queue.first, queue.second, state.thread->priority.load(), [](const i8 priority, const SyncWaiters::value_type &it) { return it.second->priority > priority; }), {key, state.thread});

            __atomic_store_n(key, true, __ATOMIC_SEQ_CST); // We need to notify any userspace threads that there are waiters on this conditional variable by writing back a boolean flag denoting it

            state.scheduler->RemoveThread();
            MutexUnlock(mutex);
        }

        if (timeout > 0 && !state.scheduler->TimedWaitSchedule(std::chrono::nanoseconds(timeout))) {
            bool inQueue{true};
            {
                // Attempt to remove ourselves from the queue so we cannot be signalled
                std::unique_lock syncLock{syncWaiterMutex};
                auto queue{syncWaiters.equal_range(key)};
                auto iterator{std::find(queue.first, queue.second, SyncWaiters::value_type{key, state.thread})};
                if (iterator != queue.second)
                    syncWaiters.erase(iterator);
                else
                    inQueue = false;
            }

            bool shouldWait{false};
            if (!inQueue) {
                // If we weren't in the queue then we need to check if we were signalled already
                while (true) {
                    std::unique_lock lock{state.thread->waiterMutex};

                    if (state.thread->waitSignalled) {
                        if (state.thread->waitThread) {
                            auto waitThread{state.thread->waitThread};
                            std::unique_lock waitLock{waitThread->waiterMutex, std::try_to_lock};
                            if (!waitLock) {
                                // If we can't lock the waitThread's waiterMutex then we need to wait without holding the current thread's waiterMutex to avoid a deadlock
                                lock.unlock();
                                waitLock.lock();
                                continue;
                            }

                            auto &waiters{waitThread->waiters};
                            auto it{std::find(waiters.begin(), waiters.end(), state.thread)};
                            if (it != waiters.end()) {
                                // If we were signalled but are waiting on locking the associated mutex then we need to cancel our wait
                                waiters.erase(it);
                                state.thread->UpdatePriorityInheritance();

                                state.thread->waitMutex = nullptr;
                                state.thread->waitTag = 0;
                                state.thread->waitThread = nullptr;
                            } else {
                                // If we were signalled and are no longer waiting on the associated mutex then we're already scheduled
                                shouldWait = true;
                            }
                        } else {
                            // If the waitThread is null then we were signalled and are no longer waiting on the associated mutex
                            shouldWait = true;
                        }
                    } else {
                        // If we were in the process of being signalled but prior to the mutex being locked then we can just cancel our wait
                        state.thread->waitConditionVariable = nullptr;
                        state.thread->waitSignalled = true;
                    }
                    break;
                }
            } else {
                // If we were in the queue then we can just cancel our wait
                state.thread->waitConditionVariable = nullptr;
                state.thread->waitSignalled = true;
            }

            if (shouldWait) {
                // Wait if we've been signalled in the meantime as it would be problematic to double insert a thread into the scheduler
                state.scheduler->WaitSchedule();
                return state.thread->waitResult;
            }

            state.scheduler->InsertThread(state.thread);
            state.scheduler->WaitSchedule();

            return result::TimedOut;
        } else {
            state.scheduler->WaitSchedule();
        }

        return state.thread->waitResult;
    }

    void KProcess::ConditionVariableSignal(u32 *key, i32 amount) {
        TRACE_EVENT_FMT("kernel", "ConditionVariableSignal {}", fmt::ptr(key));

        i32 waiterCount{amount};
        while (amount <= 0 || waiterCount) {
            std::shared_ptr<type::KThread> thread;
            void *conditionVariable{};
            {
                // Try to find a thread to signal
                std::scoped_lock lock{syncWaiterMutex};
                auto queue{syncWaiters.equal_range(key)};

                if (queue.first != queue.second) {
                    // If threads are waiting on us still then we need to remove the highest priority thread from the queue
                    auto it{std::min_element(queue.first, queue.second, [](const SyncWaiters::value_type &lhs, const SyncWaiters::value_type &rhs) { return lhs.second->priority < rhs.second->priority; })};
                    thread = it->second;
                    conditionVariable = thread->waitConditionVariable;
                    #ifndef NDEBUG
                    if (conditionVariable != key)
                        LOGW("Condition variable mismatch: {} != {}", conditionVariable, fmt::ptr(key));
                    #endif

                    syncWaiters.erase(it);
                    waiterCount--;
                } else if (queue.first == queue.second) {
                    // If we didn't find a thread then we need to clear the boolean flag denoting that there are no more threads waiting on this conditional variable
                    __atomic_store_n(key, false, __ATOMIC_SEQ_CST);
                    break;
                }
            }

            std::scoped_lock lock{thread->waiterMutex};
            if (thread->waitConditionVariable == conditionVariable) {
                // If the thread is still waiting on the same condition variable then we can signal it (It could no longer be waiting due to a timeout)
                u32 *mutex{thread->waitMutex};
                KHandle tag{thread->waitTag};

                while (true) {
                    // We need to lock the mutex before the thread can be scheduled
                    KHandle value{};
                    if (__atomic_compare_exchange_n(mutex, &value, tag, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                        // A quick CAS to lock the mutex for the thread, we can just schedule the thread if we succeed
                        state.scheduler->InsertThread(thread);
                        break;
                    }

                    if ((value & HandleWaitersBit) == 0)
                        // Set the waiters bit in the mutex if it wasn't already set
                        if (!__atomic_compare_exchange_n(mutex, &value, value | HandleWaitersBit, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                            continue; // If we failed to set the waiters bit due to an outdated value then try again

                    // If we couldn't CAS the lock then we need to let the mutex holder schedule the thread instead of us during an unlock
                    auto result{MutexLock(thread, mutex, value & ~HandleWaitersBit, tag, true)};
                    if (result == result::InvalidCurrentMemory) {
                        continue;
                    } else if (result == result::InvalidHandle) {
                        thread->waitResult = result::InvalidState;
                        state.scheduler->InsertThread(thread);
                    } else if (result != Result{}) {
                        throw exception("Failed to lock mutex: 0x{:X}", result);
                    }
                    break;
                }

                // Update the thread's wait state to avoid incorrect timeout cancellation behavior
                thread->waitConditionVariable = nullptr;
                thread->waitSignalled = true;
                thread->waitResult = {};
            }
        }
    }

    Result KProcess::WaitForAddress(u32 *address, u32 value, i64 timeout, ArbitrationType type) {
        TRACE_EVENT_FMT("kernel", "WaitForAddress {}", fmt::ptr(address));

        {
            std::scoped_lock lock{syncWaiterMutex};

            u32 userValue{__atomic_load_n(address, __ATOMIC_SEQ_CST)};
            switch (type) {
                case ArbitrationType::WaitIfLessThan:
                    if (userValue >= value) [[unlikely]]
                        return result::InvalidState;
                    break;

                case ArbitrationType::DecrementAndWaitIfLessThan: {
                    do {
                        if (value <= userValue) [[unlikely]] // We want to explicitly decrement **after** the check
                            return result::InvalidState;
                    } while (!__atomic_compare_exchange_n(address, &userValue, userValue - 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
                    break;
                }

                case ArbitrationType::WaitIfEqual:
                    if (userValue != value) [[unlikely]]
                        return result::InvalidState;
                    break;
            }

            if (timeout == 0) [[unlikely]]
                return result::TimedOut;

            auto queue{syncWaiters.equal_range(address)};
            syncWaiters.insert(std::upper_bound(queue.first, queue.second, state.thread->priority.load(), [](const i8 priority, const SyncWaiters::value_type &it) { return it.second->priority > priority; }), {address, state.thread});

            state.scheduler->RemoveThread();
        }

        if (timeout > 0 && !state.scheduler->TimedWaitSchedule(std::chrono::nanoseconds(timeout))) {
            bool shouldWait{false};
            {
                std::scoped_lock lock{syncWaiterMutex};
                auto queue{syncWaiters.equal_range(address)};
                auto iterator{std::find(queue.first, queue.second, SyncWaiters::value_type{address, state.thread})};
                if (iterator != queue.second) {
                    if (syncWaiters.erase(iterator) == queue.second)
                        // We need to update the boolean flag denoting that there are no more threads waiting on this address
                        __atomic_store_n(address, false, __ATOMIC_SEQ_CST);
                } else {
                    // If we didn't find the thread in the queue then it must have been signalled already and we should just wait
                    shouldWait = true;
                }
            }

            if (shouldWait) {
                state.scheduler->WaitSchedule(false);
                return {};
            }

            state.scheduler->InsertThread(state.thread);
            state.scheduler->WaitSchedule(false);

            return result::TimedOut;
        } else {
            state.scheduler->WaitSchedule(false);
        }

        return {};
    }

    Result KProcess::SignalToAddress(u32 *address, u32 value, i32 amount, SignalType type) {
        TRACE_EVENT_FMT("kernel", "SignalToAddress {}", fmt::ptr(address));

        std::scoped_lock lock{syncWaiterMutex};
        auto queue{syncWaiters.equal_range(address)};

        if (type != SignalType::Signal) {
            u32 newValue{value};
            if (type == SignalType::SignalAndIncrementIfEqual) {
                newValue++;
            } else if (type == SignalType::SignalAndModifyBasedOnWaitingThreadCountIfEqual) {
                if (amount <= 0) {
                    if (queue.first != queue.second)
                        newValue -= 2;
                    else
                        newValue++;
                } else {
                    if (queue.first != queue.second) {
                        i32 waiterCount{static_cast<i32>(std::distance(queue.first, queue.second))};
                        if (waiterCount < amount)
                            newValue--;
                    } else {
                        newValue++;
                    }
                }
            }

            if (!__atomic_compare_exchange_n(address, &value, newValue, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) [[unlikely]]
                return result::InvalidState;
        }

        i32 waiterCount{amount};
        boost::container::small_vector<SyncWaiters::iterator, 10> orderedThreads;
        for (auto it{queue.first}; it != queue.second; it++) {
            // While threads should generally be inserted in priority order, they may not always be due to the way the kernel handles priority updates
            // As a result, we need to create a second list of threads ordered by priority to ensure that we wake up the highest priority threads first
            auto thread{it->second};
            orderedThreads.insert(std::upper_bound(orderedThreads.begin(), orderedThreads.end(), thread->priority.load(), [](const i8 priority, const SyncWaiters::iterator &it) { return it->second->priority > priority; }), it);
        }

        for (auto &it : orderedThreads) {
            auto thread{it->second};

            syncWaiters.erase(it);
            state.scheduler->InsertThread(thread);

            if (--waiterCount == 0 && amount > 0)
                break;
        }

        return {};
    }
}
