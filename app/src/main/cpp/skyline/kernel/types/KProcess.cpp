// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <kernel/results.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KProcess::TlsPage::TlsPage(const std::shared_ptr<KPrivateMemory> &memory) : memory(memory) {}

    u8 *KProcess::TlsPage::ReserveSlot() {
        if (index == constant::TlsSlots)
            return nullptr;
        return memory->ptr + (constant::TlsSlotSize * index++);
    }

    KProcess::KProcess(const DeviceState &state) : memory(state), KSyncObject(state, KType::KProcess) {}

    KProcess::~KProcess() {
        std::lock_guard guard(threadMutex);
        disableThreadCreation = true;
        for (const auto &thread : threads)
            thread->Kill(true);
    }

    void KProcess::Kill(bool join, bool all, bool disableCreation) {
        std::lock_guard guard(threadMutex);
        if (disableCreation)
            disableThreadCreation = true;
        if (all) {
            for (const auto &thread : threads)
                thread->Kill(join);
        } else {
            std::shared_ptr<KThread> thread;
            try {
                thread = threads.at(0);
            } catch (const std::out_of_range &) {
                return;
            }
            thread->Kill(join);
        }
    }

    void KProcess::InitializeHeapTls() {
        constexpr size_t DefaultHeapSize{0x200000};
        heap = heap.make_shared(state, reinterpret_cast<u8 *>(state.process->memory.heap.address), DefaultHeapSize, memory::Permission{true, true, false}, memory::states::Heap);
        InsertItem(heap); // Insert it into the handle table so GetMemoryObject will contain it
        tlsExceptionContext = AllocateTlsSlot();
    }

    u8 *KProcess::AllocateTlsSlot() {
        std::lock_guard lock(tlsMutex);
        u8 *slot;
        for (auto &tlsPage: tlsPages)
            if ((slot = tlsPage->ReserveSlot()))
                return slot;

        slot = tlsPages.empty() ? reinterpret_cast<u8 *>(memory.tlsIo.address) : ((*(tlsPages.end() - 1))->memory->ptr + PAGE_SIZE);
        auto tlsPage{std::make_shared<TlsPage>(std::make_shared<KPrivateMemory>(state, slot, PAGE_SIZE, memory::Permission(true, true, false), memory::states::ThreadLocal))};
        tlsPages.push_back(tlsPage);
        return tlsPage->ReserveSlot();
    }

    std::shared_ptr<KThread> KProcess::CreateThread(void *entry, u64 argument, void *stackTop, std::optional<u8> priority, std::optional<u8> idealCore) {
        std::lock_guard guard(threadMutex);
        if (disableThreadCreation)
            return nullptr;
        if (!stackTop && threads.empty()) { //!< Main thread stack is created by the kernel and owned by the process
            mainThreadStack = mainThreadStack.make_shared(state, reinterpret_cast<u8 *>(state.process->memory.stack.address), state.process->npdm.meta.mainThreadStackSize, memory::Permission{true, true, false}, memory::states::Stack);
            if (mprotect(mainThreadStack->ptr, PAGE_SIZE, PROT_NONE))
                throw exception("Failed to create guard page for thread stack at 0x{:X}", mainThreadStack->ptr);
            stackTop = mainThreadStack->ptr + mainThreadStack->size;
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
                        if (mem->IsInside(ptr))
                            return std::make_optional<KProcess::HandleOut<KMemory>>({mem, constant::BaseHandleIndex + index});
                    }

                    default:
                        break;
                }
            }
        }
        return std::nullopt;
    }

    constexpr u32 HandleWaitersBit{0b01000000000000000000000000000000}; //!< A bit which denotes if a mutex psuedo-handle has waiters or not

    Result KProcess::MutexLock(u32 *mutex, KHandle ownerHandle, KHandle tag) {
        std::shared_ptr<KThread> owner;
        try {
            owner = GetHandle<KThread>(ownerHandle);
        } catch (const std::out_of_range &) {
            return result::InvalidHandle;
        }

        bool isHighestPriority;
        {
            std::lock_guard lock(owner->waiterMutex);

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

            std::atomic_store(&state.thread->waitThread, owner);
            state.thread->waitKey = mutex;
            state.thread->waitTag = tag;
        }

        if (isHighestPriority) {
            // If we were the highest priority thread then we need to inherit priorities for all threads we're waiting on recursively
            do {
                u8 priority, ownerPriority;
                do {
                    // Try to CAS the priority of the owner with the current thread
                    // If they're equivalent then we don't need to CAS as the priority won't be inherited
                    ownerPriority = owner->priority.load();
                    priority = std::min(ownerPriority, state.thread->priority.load());
                } while (ownerPriority != priority && owner->priority.compare_exchange_strong(ownerPriority, priority));

                if (ownerPriority != priority) {
                    auto waitThread{std::atomic_load(&owner->waitThread)};
                    while (waitThread) {
                        std::lock_guard lock(waitThread->waiterMutex);

                        auto currentWaitThread{std::atomic_load(&owner->waitThread)};
                        if (waitThread != currentWaitThread) {
                            waitThread = currentWaitThread;
                            continue;
                        }

                        // We need to update the location of the owner thread in the waiter queue of the thread it's waiting on
                        auto &waiters{waitThread->waiters};
                        waiters.erase(std::find(waiters.begin(), waiters.end(), waitThread));
                        waiters.insert(std::upper_bound(waiters.begin(), waiters.end(), state.thread->priority.load(), KThread::IsHigherPriority), owner);
                        break;
                    }

                    state.scheduler->UpdatePriority(owner);

                    owner = waitThread;
                } else {
                    break;
                }
            } while (owner);
        }

        state.scheduler->WaitSchedule(false);

        return {};
    }

    void KProcess::MutexUnlock(u32 *mutex) {
        std::lock_guard lock(state.thread->waiterMutex);
        auto &waiters{state.thread->waiters};
        auto nextOwnerIt{std::find_if(waiters.begin(), waiters.end(), [mutex](const std::shared_ptr<KThread> &thread) { return thread->waitKey == mutex; })};
        if (nextOwnerIt != waiters.end()) {
            auto nextOwner{*nextOwnerIt};
            std::lock_guard nextLock(nextOwner->waiterMutex);
            std::atomic_store(&nextOwner->waitThread, std::shared_ptr<KThread>{nullptr});
            nextOwner->waitKey = nullptr;

            // Move all threads waiting on this key to the next owner's waiter list
            std::shared_ptr<KThread> nextWaiter{};
            for (auto it{waiters.erase(nextOwnerIt)}, nextIt{std::next(it)}; it != waiters.end(); it = nextIt++) {
                auto thread{*it};
                if (thread->waitKey == mutex) {
                    nextOwner->waiters.splice(std::upper_bound(nextOwner->waiters.begin(), nextOwner->waiters.end(), (*it)->priority.load(), KThread::IsHigherPriority), waiters, it);
                    std::atomic_store(&thread->waitThread, nextOwner);
                    if (!nextWaiter)
                        nextWaiter = thread;
                }
            }

            if (!waiters.empty()) {
                // If there are threads still waiting on us then try to inherit their priority
                auto highestPriority{waiters.front()};
                u8 priority, ownerPriority;
                do {
                    ownerPriority = state.thread->priority.load();
                    priority = std::min(ownerPriority, highestPriority->priority.load());
                } while (ownerPriority != priority && nextOwner->priority.compare_exchange_strong(ownerPriority, priority));
                state.scheduler->UpdatePriority(state.thread);
            }

            if (nextWaiter) {
                // If there is a waiter on the new owner then try to inherit it's priority
                u8 priority, ownerPriority;
                do {
                    ownerPriority = nextOwner->priority.load();
                    priority = std::min(ownerPriority, nextWaiter->priority.load());
                } while (ownerPriority != priority && nextOwner->priority.compare_exchange_strong(ownerPriority, priority));

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
        {
            std::lock_guard lock(syncWaiterMutex);
            auto queue{syncWaiters.equal_range(key)};
            syncWaiters.insert(std::upper_bound(queue.first, queue.second, state.thread->priority.load(), [](const i8 priority, const SyncWaiters::value_type &it) { return it.second->priority > priority; }), {key, state.thread});

            __atomic_store_n(key, true, __ATOMIC_SEQ_CST); // We need to notify any userspace threads that there are waiters on this conditional variable by writing back a boolean flag denoting it

            state.scheduler->RemoveThread();
            MutexUnlock(mutex);
            __sync_synchronize();
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

        while (true) {
            KHandle value{};
            if (__atomic_compare_exchange_n(mutex, &value, tag, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                return {};
            if (!(value & HandleWaitersBit))
                if (!__atomic_compare_exchange_n(mutex, &value, value | HandleWaitersBit, false, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))
                    continue;
            if (MutexLock(mutex, value & ~HandleWaitersBit, tag) == Result{})
                return {};
        }
    }

    void KProcess::ConditionalVariableSignal(u32 *key, u64 amount) {
        std::lock_guard lock(syncWaiterMutex);
        auto queue{syncWaiters.equal_range(key)};
        auto it{queue.first};
        if (queue.first != queue.second)
            for (; it != queue.second && amount; it = syncWaiters.erase(it), amount--)
                state.scheduler->InsertThread(it->second);
        if (it == queue.second)
            __atomic_store_n(key, false, __ATOMIC_SEQ_CST); // We need to update the boolean flag denoting that there are no more threads waiting on this conditional variable
    }
}
