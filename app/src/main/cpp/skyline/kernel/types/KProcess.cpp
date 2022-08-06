// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <common/trace.h>
#include <kernel/results.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KProcess::TlsPage::TlsPage(std::shared_ptr<KPrivateMemory> memory) : memory(std::move(memory)) {}

    u8 *KProcess::TlsPage::ReserveSlot() {
        if (index == constant::TlsSlots)
            return nullptr;
        return memory->guest.data() + (constant::TlsSlotSize * index++);
    }

    KProcess::KProcess(const DeviceState &state) : memory(state), KSyncObject(state, KType::KProcess) {}

    KProcess::~KProcess() {
        std::scoped_lock guard{threadMutex};
        disableThreadCreation = true;
        for (const auto &thread : threads)
            thread->Kill(true);
    }

    void KProcess::Kill(bool join, bool all, bool disableCreation) {
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
        heap = std::make_shared<KPrivateMemory>(state, span<u8>{state.process->memory.heap.data(), DefaultHeapSize}, memory::Permission{true, true, false}, memory::states::Heap);
        InsertItem(heap); // Insert it into the handle table so GetMemoryObject will contain it
        tlsExceptionContext = AllocateTlsSlot();
    }

    u8 *KProcess::AllocateTlsSlot() {
        std::scoped_lock lock{tlsMutex};
        u8 *slot;
        for (auto &tlsPage : tlsPages)
            if ((slot = tlsPage->ReserveSlot()))
                return slot;

        slot = tlsPages.empty() ? reinterpret_cast<u8 *>(memory.tlsIo.data()) : ((*(tlsPages.end() - 1))->memory->guest.data() + PAGE_SIZE);
        auto tlsPage{std::make_shared<TlsPage>(std::make_shared<KPrivateMemory>(state, span<u8>{slot, PAGE_SIZE}, memory::Permission(true, true, false), memory::states::ThreadLocal))};
        tlsPages.push_back(tlsPage);
        return tlsPage->ReserveSlot();
    }

    std::shared_ptr<KThread> KProcess::CreateThread(void *entry, u64 argument, void *stackTop, std::optional<i8> priority, std::optional<u8> idealCore) {
        std::scoped_lock guard{threadMutex};
        if (disableThreadCreation)
            return nullptr;
        if (!stackTop && threads.empty()) { //!< Main thread stack is created by the kernel and owned by the process
            mainThreadStack = std::make_shared<KPrivateMemory>(state, span<u8>{state.process->memory.stack.data(), state.process->npdm.meta.mainThreadStackSize}, memory::Permission{true, true, false}, memory::states::Stack);
            stackTop = mainThreadStack->guest.end().base();
        }
        auto thread{NewHandle<KThread>(this, threads.size(), entry, argument, stackTop, priority ? *priority : state.process->npdm.meta.mainThreadPriority, idealCore ? *idealCore : state.process->npdm.meta.idealCore).item};
        threads.push_back(thread);
        return thread;
    }

    std::optional<KProcess::HandleOut<KMemory>> KProcess::GetMemoryObject(u8 *ptr) {
        std::shared_lock lock(handleMutex);

        for (KHandle index{}; index < handles.size(); index++) {
            auto &object{handles[index]};
            if (object) {
                switch (object->objectType) {
                    case type::KType::KPrivateMemory:
                    case type::KType::KSharedMemory:
                    case type::KType::KTransferMemory: {
                        auto mem{std::static_pointer_cast<type::KMemory>(object)};
                        if (mem->guest.contains(ptr))
                            return std::make_optional<KProcess::HandleOut<KMemory>>({mem, constant::BaseHandleIndex + index});
                    }

                    default:
                        break;
                }
            }
        }
        return std::nullopt;
    }

    void KProcess::ClearHandleTable() {
        std::shared_lock lock(handleMutex);
        handles.clear();
    }

    constexpr u32 HandleWaitersBit{1UL << 30}; //!< A bit which denotes if a mutex psuedo-handle has waiters or not

    Result KProcess::MutexLock(u32 *mutex, KHandle ownerHandle, KHandle tag) {
        TRACE_EVENT_FMT("kernel", "MutexLock 0x{:X}", mutex);

        std::shared_ptr<KThread> owner;
        try {
            owner = GetHandle<KThread>(ownerHandle);
        } catch (const std::out_of_range &) {
            if (*mutex != (ownerHandle | HandleWaitersBit))
                return result::InvalidCurrentMemory;

            return result::InvalidHandle;
        }

        bool isHighestPriority;
        {
            std::scoped_lock lock{owner->waiterMutex, state.thread->waiterMutex}; // We need to lock both mutexes at the same time as we mutate the owner and the current thread, the ordering of locks **must** match MutexUnlock to avoid deadlocks

            u32 value{};
            if (__atomic_compare_exchange_n(mutex, &value, tag, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                // We try to do a CAS to get ownership of the mutex in the case that it's unoccupied
                return {};
            if (value != (ownerHandle | HandleWaitersBit))
                // We ensure that the mutex's value is the handle with the waiter bit set
                return result::InvalidCurrentMemory;

            auto &waiters{owner->waiters};
            isHighestPriority = waiters.insert(std::upper_bound(waiters.begin(), waiters.end(), state.thread->priority.load(), KThread::IsHigherPriority), state.thread) == waiters.begin();
            state.scheduler->RemoveThread();

            state.thread->waitThread = owner;
            state.thread->waitKey = mutex;
            state.thread->waitTag = tag;
        }

        if (isHighestPriority)
            // If we were the highest priority thread then we need to inherit priorities for all threads we're waiting on recursively
            state.thread->UpdatePriorityInheritance();

        state.scheduler->WaitSchedule(false);

        return {};
    }

    void KProcess::MutexUnlock(u32 *mutex) {
        TRACE_EVENT_FMT("kernel", "MutexUnlock 0x{:X}", mutex);

        std::scoped_lock lock{state.thread->waiterMutex};
        auto &waiters{state.thread->waiters};
        auto nextOwnerIt{std::find_if(waiters.begin(), waiters.end(), [mutex](const std::shared_ptr<KThread> &thread) { return thread->waitKey == mutex; })};
        if (nextOwnerIt != waiters.end()) {
            auto nextOwner{*nextOwnerIt};
            std::scoped_lock nextLock{nextOwner->waiterMutex};
            nextOwner->waitThread = std::shared_ptr<KThread>{nullptr};
            nextOwner->waitKey = nullptr;

            // Move all threads waiting on this key to the next owner's waiter list
            std::shared_ptr<KThread> nextWaiter{};
            for (auto it{waiters.erase(nextOwnerIt)}, nextIt{std::next(it)}; it != waiters.end(); it = nextIt++) {
                auto thread{*it};
                if (thread->waitKey == mutex) {
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

    Result KProcess::ConditionalVariableWait(u32 *key, u32 *mutex, KHandle tag, i64 timeout) {
        TRACE_EVENT_FMT("kernel", "ConditionalVariableWait 0x{:X} (0x{:X})", key, mutex);

        {
            std::scoped_lock lock{syncWaiterMutex};
            auto queue{syncWaiters.equal_range(key)};
            syncWaiters.insert(std::upper_bound(queue.first, queue.second, state.thread->priority.load(), [](const i8 priority, const SyncWaiters::value_type &it) { return it.second->priority > priority; }), {key, state.thread});

            __atomic_store_n(key, true, __ATOMIC_SEQ_CST); // We need to notify any userspace threads that there are waiters on this conditional variable by writing back a boolean flag denoting it

            state.scheduler->RemoveThread();
            MutexUnlock(mutex);
        }

        if (timeout > 0 && !state.scheduler->TimedWaitSchedule(std::chrono::nanoseconds(timeout))) {
            std::unique_lock lock(syncWaiterMutex);
            auto queue{syncWaiters.equal_range(key)};
            auto iterator{std::find(queue.first, queue.second, SyncWaiters::value_type{key, state.thread})};
            if (iterator != queue.second)
                if (syncWaiters.erase(iterator) == queue.second)
                    __atomic_store_n(key, false, __ATOMIC_SEQ_CST);

            lock.unlock();
            state.scheduler->InsertThread(state.thread);
            state.scheduler->WaitSchedule();

            return result::TimedOut;
        } else {
            state.scheduler->WaitSchedule(false);
        }

        KHandle value{};
        if (!__atomic_compare_exchange_n(mutex, &value, tag, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            while (MutexLock(mutex, value & ~HandleWaitersBit, tag) != Result{})
                if ((value = __atomic_or_fetch(mutex, HandleWaitersBit, __ATOMIC_SEQ_CST)) == HandleWaitersBit)
                    if (__atomic_compare_exchange_n(mutex, &value, tag, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                        break;

        return {};
    }

    void KProcess::ConditionalVariableSignal(u32 *key, i32 amount) {
        TRACE_EVENT_FMT("kernel", "ConditionalVariableSignal 0x{:X}", key);

        std::scoped_lock lock{syncWaiterMutex};
        auto queue{syncWaiters.equal_range(key)};

        auto it{queue.first};
        for (i32 waiterCount{amount}; it != queue.second && (amount <= 0 || waiterCount); it = syncWaiters.erase(it), waiterCount--)
            state.scheduler->InsertThread(it->second);

        if (it == queue.second)
            __atomic_store_n(key, false, __ATOMIC_SEQ_CST); // We need to update the boolean flag denoting that there are no more threads waiting on this conditional variable
    }

    Result KProcess::WaitForAddress(u32 *address, u32 value, i64 timeout, ArbitrationType type) {
        TRACE_EVENT_FMT("kernel", "WaitForAddress 0x{:X}", address);

        {
            std::scoped_lock lock{syncWaiterMutex};

            switch (type) {
                case ArbitrationType::WaitIfLessThan:
                    if (*address >= value) [[unlikely]]
                        return result::InvalidState;
                    break;

                case ArbitrationType::DecrementAndWaitIfLessThan: {
                    u32 userValue{__atomic_load_n(address, __ATOMIC_SEQ_CST)};
                    do {
                        if (value <= userValue) [[unlikely]] // We want to explicitly decrement **after** the check
                            return result::InvalidState;
                    } while (!__atomic_compare_exchange_n(address, &userValue, userValue - 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
                    break;
                }

                case ArbitrationType::WaitIfEqual:
                    if (*address != value) [[unlikely]]
                        return result::InvalidState;
                    break;
            }

            auto queue{syncWaiters.equal_range(address)};
            syncWaiters.insert(std::upper_bound(queue.first, queue.second, state.thread->priority.load(), [](const i8 priority, const SyncWaiters::value_type &it) { return it.second->priority > priority; }), {address, state.thread});

            state.scheduler->RemoveThread();
        }

        if (timeout > 0 && !state.scheduler->TimedWaitSchedule(std::chrono::nanoseconds(timeout))) {
            {
                std::scoped_lock lock{syncWaiterMutex};
                auto queue{syncWaiters.equal_range(address)};
                auto iterator{std::find(queue.first, queue.second, SyncWaiters::value_type{address, state.thread})};
                if (iterator != queue.second)
                    if (syncWaiters.erase(iterator) == queue.second)
                        __atomic_store_n(address, false, __ATOMIC_SEQ_CST);
            }

            state.scheduler->InsertThread(state.thread);
            state.scheduler->WaitSchedule();

            return result::TimedOut;
        } else {
            state.scheduler->WaitSchedule(false);
        }

        return {};
    }

    Result KProcess::SignalToAddress(u32 *address, u32 value, i32 amount, SignalType type) {
        TRACE_EVENT_FMT("kernel", "SignalToAddress 0x{:X}", address);

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
        for (auto it{queue.first}; it != queue.second && (amount <= 0 || waiterCount); it = syncWaiters.erase(it), waiterCount--)
            state.scheduler->InsertThread(it->second);

        return {};
    }
}
