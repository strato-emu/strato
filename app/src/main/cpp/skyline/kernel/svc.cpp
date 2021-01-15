// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <kernel/types/KProcess.h>
#include <vfs/npdm.h>
#include "results.h"
#include "svc.h"

namespace skyline::kernel::svc {
    void SetHeapSize(const DeviceState &state) {
        u32 size{state.ctx->gpr.w1};

        if (!util::IsAligned(size, 0x200000)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.ctx->gpr.x1 = 0;

            state.logger->Warn("svcSetHeapSize: 'size' not divisible by 2MB: {}", size);
            return;
        }

        auto &heap{state.process->heap};
        heap->Resize(size);

        state.ctx->gpr.w0 = Result{};
        state.ctx->gpr.x1 = reinterpret_cast<u64>(heap->ptr);

        state.logger->Debug("svcSetHeapSize: Allocated at 0x{:X} - 0x{:X} (0x{:X} bytes)", heap->ptr, heap->ptr + heap->size, heap->size);
    }

    void SetMemoryAttribute(const DeviceState &state) {
        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        if (!util::PageAligned(pointer)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcSetMemoryAttribute: 'pointer' not page aligned: 0x{:X}", pointer);
            return;
        }

        size_t size{state.ctx->gpr.x1};
        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcSetMemoryAttribute: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        memory::MemoryAttribute mask{.value = state.ctx->gpr.w2};
        memory::MemoryAttribute value{.value = state.ctx->gpr.w3};

        auto maskedValue{mask.value | value.value};
        if (maskedValue != mask.value || !mask.isUncached || mask.isDeviceShared || mask.isBorrowed || mask.isIpcLocked) {
            state.ctx->gpr.w0 = result::InvalidCombination;
            state.logger->Warn("svcSetMemoryAttribute: 'mask' invalid: 0x{:X}, 0x{:X}", mask.value, value.value);
            return;
        }

        auto chunk{state.process->memory.Get(pointer)};
        if (!chunk) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcSetMemoryAttribute: Cannot find memory region: 0x{:X}", pointer);
            return;
        }

        if (!chunk->state.attributeChangeAllowed) {
            state.ctx->gpr.w0 = result::InvalidState;
            state.logger->Warn("svcSetMemoryAttribute: Attribute change not allowed for chunk: 0x{:X}", pointer);
            return;
        }

        auto newChunk{*chunk};
        newChunk.ptr = pointer;
        newChunk.size = size;
        newChunk.attributes.isUncached = value.isUncached;
        state.process->memory.InsertChunk(newChunk);

        state.logger->Debug("svcSetMemoryAttribute: Set CPU caching to {} at 0x{:X} - 0x{:X} (0x{:X} bytes)", !static_cast<bool>(value.isUncached), pointer, pointer + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void MapMemory(const DeviceState &state) {
        auto destination{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        auto source{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        size_t size{state.ctx->gpr.x2};

        if (!util::PageAligned(destination) || !util::PageAligned(source)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcMapMemory: Addresses not page aligned: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcMapMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        auto stack{state.process->memory.stack};
        if (!stack.IsInside(destination)) {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            state.logger->Warn("svcMapMemory: Destination not within stack region: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        auto chunk{state.process->memory.Get(source)};
        if (!chunk) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcMapMemory: Source has no descriptor: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }
        if (!chunk->state.mapAllowed) {
            state.ctx->gpr.w0 = result::InvalidState;
            state.logger->Warn("svcMapMemory: Source doesn't allow usage of svcMapMemory: Source: 0x{:X}, Destination: 0x{:X}, Size: 0x{:X}, MemoryState: 0x{:X}", source, destination, size, chunk->state.value);
            return;
        }

        state.process->NewHandle<type::KPrivateMemory>(destination, size, chunk->permission, memory::states::Stack);
        std::memcpy(destination, source, size);

        auto object{state.process->GetMemoryObject(source)};
        if (!object)
            throw exception("svcMapMemory: Cannot find memory object in handle table for address 0x{:X}", source);
        object->item->UpdatePermission(source, size, {false, false, false});

        state.logger->Debug("svcMapMemory: Mapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", source, source + size, destination, destination + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void UnmapMemory(const DeviceState &state) {
        auto source{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        auto destination{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        size_t size{state.ctx->gpr.x2};

        if (!util::PageAligned(destination) || !util::PageAligned(source)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcUnmapMemory: Addresses not page aligned: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcUnmapMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        auto stack{state.process->memory.stack};
        if (!stack.IsInside(source)) {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            state.logger->Warn("svcUnmapMemory: Source not within stack region: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        auto sourceChunk{state.process->memory.Get(source)};
        auto destChunk{state.process->memory.Get(destination)};
        if (!sourceChunk || !destChunk) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcUnmapMemory: Addresses have no descriptor: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        if (!destChunk->state.mapAllowed) {
            state.ctx->gpr.w0 = result::InvalidState;
            state.logger->Warn("svcUnmapMemory: Destination doesn't allow usage of svcMapMemory: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes) 0x{:X}", source, destination, size, destChunk->state.value);
            return;
        }

        auto destObject{state.process->GetMemoryObject(destination)};
        if (!destObject)
            throw exception("svcUnmapMemory: Cannot find destination memory object in handle table for address 0x{:X}", destination);

        destObject->item->UpdatePermission(destination, size, sourceChunk->permission);

        std::memcpy(source, destination, size);

        auto sourceObject{state.process->GetMemoryObject(source)};
        if (!sourceObject)
            throw exception("svcUnmapMemory: Cannot find source memory object in handle table for address 0x{:X}", source);

        state.process->CloseHandle(sourceObject->handle);

        state.logger->Debug("svcUnmapMemory: Unmapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", source, source + size, destination, destination + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void QueryMemory(const DeviceState &state) {
        memory::MemoryInfo memInfo{};

        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x2)};
        auto chunk{state.process->memory.Get(pointer)};

        if (chunk) {
            memInfo = {
                .address = reinterpret_cast<u64>(chunk->ptr),
                .size = chunk->size,
                .type = static_cast<u32>(chunk->state.type),
                .attributes = chunk->attributes.value,
                .permissions = static_cast<u32>(chunk->permission.Get()),
                .deviceRefCount = 0,
                .ipcRefCount = 0,
            };

            state.logger->Debug("svcQueryMemory: Address: 0x{:X}, Region Start: 0x{:X}, Size: 0x{:X}, Type: 0x{:X}, Is Uncached: {}, Permissions: {}{}{}", pointer, memInfo.address, memInfo.size, memInfo.type, static_cast<bool>(chunk->attributes.isUncached), chunk->permission.r ? 'R' : '-', chunk->permission.w ? 'W' : '-', chunk->permission.x ? 'X' : '-');
        } else {
            auto addressSpaceEnd{reinterpret_cast<u64>(state.process->memory.addressSpace.address + state.process->memory.addressSpace.size)};

            memInfo = {
                .address = addressSpaceEnd,
                .size = ~addressSpaceEnd + 1,
                .type = static_cast<u32>(memory::MemoryType::Reserved),
            };

            state.logger->Debug("svcQueryMemory: Trying to query memory outside of the application's address space: 0x{:X}", pointer);
        }

        *reinterpret_cast<memory::MemoryInfo *>(state.ctx->gpr.x0) = memInfo;

        state.ctx->gpr.w0 = Result{};
    }

    void ExitProcess(const DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting process");
        if (state.thread->id)
            state.process->Kill(false);
        std::longjmp(state.thread->originalCtx, true);
    }

    constexpr i32 IdealCoreDontCare{-1};
    constexpr i32 IdealCoreUseProcessValue{-2};
    constexpr i32 IdealCoreNoUpdate{-3};

    void CreateThread(const DeviceState &state) {
        auto entry{reinterpret_cast<void *>(state.ctx->gpr.x1)};
        auto entryArgument{state.ctx->gpr.x2};
        auto stackTop{reinterpret_cast<u8 *>(state.ctx->gpr.x3)};
        auto priority{static_cast<i8>(static_cast<u32>(state.ctx->gpr.w4))};
        auto idealCore{static_cast<i8>(static_cast<u32>(state.ctx->gpr.w5))};

        idealCore = (idealCore == IdealCoreUseProcessValue) ? state.process->npdm.meta.idealCore : idealCore;
        if (idealCore < 0 || idealCore >= constant::CoreCount) {
            state.ctx->gpr.w0 = result::InvalidCoreId;
            state.logger->Warn("svcCreateThread: 'idealCore' invalid: {}", idealCore);
            return;
        }

        if (!state.process->npdm.threadInfo.priority.Valid(priority)) {
            state.ctx->gpr.w0 = result::InvalidPriority;
            state.logger->Warn("svcCreateThread: 'priority' invalid: {}", priority);
            return;
        }

        auto stack{state.process->GetMemoryObject(stackTop)};
        if (!stack)
            throw exception("svcCreateThread: Cannot find memory object in handle table for thread stack: 0x{:X}", stackTop);

        auto thread{state.process->CreateThread(entry, entryArgument, stackTop, priority, idealCore)};
        if (thread) {
            state.logger->Debug("svcCreateThread: Created thread #{} with handle 0x{:X} (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, Ideal Core: {})", thread->id, thread->handle, entry, entryArgument, stackTop, priority, idealCore);

            state.ctx->gpr.w1 = thread->handle;
            state.ctx->gpr.w0 = Result{};
        } else {
            state.logger->Debug("svcCreateThread: Cannot create thread (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, Ideal Core: {})", entry, entryArgument, stackTop, priority, idealCore);
            state.ctx->gpr.w1 = 0;
            state.ctx->gpr.w0 = result::OutOfResource;
        }
    }

    void StartThread(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            state.logger->Debug("svcStartThread: Starting thread #{}: 0x{:X}", thread->id, handle);
            thread->Start();
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcStartThread: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ExitThread(const DeviceState &state) {
        state.logger->Debug("svcExitThread: Exiting current thread");
        std::longjmp(state.thread->originalCtx, true);
    }

    void SleepThread(const DeviceState &state) {
        constexpr i64 yieldWithoutCoreMigration{0};
        constexpr i64 yieldWithCoreMigration{-1};
        constexpr i64 yieldToAnyThread{-2};

        i64 in{static_cast<i64>(state.ctx->gpr.x0)};
        if (in > 0) {
            state.logger->Debug("svcSleepThread: Thread sleeping for {} ns", in);

            struct timespec spec{
                .tv_sec = static_cast<time_t>(in / 1000000000),
                .tv_nsec = static_cast<long>(in % 1000000000),
            };

            SchedulerScopedLock schedulerLock(state);
            nanosleep(&spec, nullptr);
        } else {
            switch (in) {
                case yieldWithCoreMigration:
                    state.logger->Debug("svcSleepThread: Waking any appropriate parked threads");
                    state.scheduler->WakeParkedThread();
                case yieldWithoutCoreMigration:
                    state.logger->Debug("svcSleepThread: Cooperative Yield");
                    state.scheduler->Rotate();
                    state.scheduler->WaitSchedule();
                    break;

                case yieldToAnyThread:
                    state.logger->Debug("svcSleepThread: Parking current thread");
                    state.scheduler->ParkThread();
                    state.scheduler->WaitSchedule(false);
                    break;

                default:
                    break;
            }
        }
    }

    void GetThreadPriority(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w1};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            u8 priority{thread->priority};
            state.logger->Debug("svcGetThreadPriority: Retrieving thread #{}'s priority: {}", thread->id, priority);

            state.ctx->gpr.w1 = priority;
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcGetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void SetThreadPriority(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        u8 priority{static_cast<u8>(state.ctx->gpr.w1)};
        if (!state.process->npdm.threadInfo.priority.Valid(priority)) {
            state.logger->Warn("svcSetThreadPriority: 'priority' invalid: 0x{:X}", priority);
            state.ctx->gpr.w0 = result::InvalidPriority;
            return;
        }
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            state.logger->Debug("svcSetThreadPriority: Setting thread #{}'s priority to {}", thread->id, priority);
            if (thread->priority != priority) {
                thread->basePriority = priority;
                u8 newPriority{};
                do {
                    // Try to CAS the priority of the thread with it's new base priority
                    // If the new priority is equivalent to the current priority then we don't need to CAS
                    newPriority = thread->priority.load();
                    newPriority = std::min(newPriority, priority);
                } while (newPriority != priority && thread->priority.compare_exchange_strong(newPriority, priority));
                state.scheduler->UpdatePriority(thread);
            }
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcSetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void GetThreadCoreMask(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w2};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            auto idealCore{thread->idealCore};
            auto affinityMask{thread->affinityMask};
            state.logger->Debug("svcGetThreadCoreMask: Getting thread #{}'s Ideal Core ({}) + Affinity Mask ({})", thread->id, idealCore, affinityMask);

            state.ctx->gpr.x2 = affinityMask.to_ullong();
            state.ctx->gpr.w1 = idealCore;
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcGetThreadCoreMask: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void SetThreadCoreMask(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        i32 idealCore{static_cast<i32>(state.ctx->gpr.w1)};
        CoreMask affinityMask{state.ctx->gpr.x2};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};

            if (idealCore == IdealCoreUseProcessValue) {
                idealCore = state.process->npdm.meta.idealCore;
                affinityMask.reset().set(idealCore);
            } else if (idealCore == IdealCoreNoUpdate) {
                idealCore = thread->idealCore;
            } else if (idealCore == IdealCoreDontCare) {
                idealCore = __builtin_ctzll(affinityMask.to_ullong()); // The first enabled core in the affinity mask
            }

            auto processMask{state.process->npdm.threadInfo.coreMask};
            if ((processMask | affinityMask) != processMask) {
                state.logger->Warn("svcSetThreadCoreMask: 'affinityMask' invalid: {} (Process Mask: {})", affinityMask, processMask);
                state.ctx->gpr.w0 = result::InvalidCoreId;
                return;
            }

            if (affinityMask.none() || !affinityMask.test(idealCore)) {
                state.logger->Warn("svcSetThreadCoreMask: 'affinityMask' invalid: {} (Ideal Core: {})", affinityMask, idealCore);
                state.ctx->gpr.w0 = result::InvalidCombination;
                return;
            }

            state.logger->Debug("svcSetThreadCoreMask: Setting thread #{}'s Ideal Core ({}) + Affinity Mask ({})", thread->id, idealCore, affinityMask);

            thread->idealCore = idealCore;
            thread->affinityMask = affinityMask;

            if (!affinityMask.test(thread->coreId)) {
                state.logger->Debug("svcSetThreadCoreMask: Migrating thread #{} to Ideal Core C{} -> C{}", thread->id, thread->coreId, idealCore);

                if (thread == state.thread) {
                    state.scheduler->RemoveThread();
                    state.scheduler->InsertThread(state.thread);
                    state.scheduler->WaitSchedule();
                } else if (!thread->running) {
                    thread->coreId = idealCore;
                } else {
                    throw exception("svcSetThreadCoreMask: Migrating a running thread due to a new core mask is not supported");
                }
            }

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcSetThreadCoreMask: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void GetCurrentProcessorNumber(const DeviceState &state) {
        auto coreId{state.thread->coreId};
        state.logger->Debug("svcGetCurrentProcessorNumber: C{}", coreId);
        state.ctx->gpr.w0 = coreId;
    }

    void ClearEvent(const DeviceState &state) {
        auto object{state.process->GetHandle<type::KEvent>(state.ctx->gpr.w0)};
        object->signalled = false;
        state.ctx->gpr.w0 = Result{};
    }

    void MapSharedMemory(const DeviceState &state) {
        try {
            auto object{state.process->GetHandle<type::KSharedMemory>(state.ctx->gpr.w0)};
            auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};

            if (!util::PageAligned(pointer)) {
                state.ctx->gpr.w0 = result::InvalidAddress;
                state.logger->Warn("svcMapSharedMemory: 'pointer' not page aligned: 0x{:X}", pointer);
                return;
            }

            size_t size{state.ctx->gpr.x2};
            if (!util::PageAligned(size)) {
                state.ctx->gpr.w0 = result::InvalidSize;
                state.logger->Warn("svcMapSharedMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
                return;
            }

            memory::Permission permission(state.ctx->gpr.w3);
            if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
                state.logger->Warn("svcMapSharedMemory: 'permission' invalid: {}{}{}", permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');
                state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
                return;
            }

            state.logger->Debug("svcMapSharedMemory: Mapping shared memory at 0x{:X} - 0x{:X} (0x{:X} bytes) ({}{}{})", pointer, pointer + size, size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

            object->Map(pointer, size, permission);

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcMapSharedMemory: 'handle' invalid: 0x{:X}", static_cast<u32>(state.ctx->gpr.w0));
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void CreateTransferMemory(const DeviceState &state) {
        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        if (!util::PageAligned(pointer)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcCreateTransferMemory: 'pointer' not page aligned: 0x{:X}", pointer);
            return;
        }

        size_t size{state.ctx->gpr.x2};
        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcCreateTransferMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        memory::Permission permission(state.ctx->gpr.w3);
        if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
            state.logger->Warn("svcCreateTransferMemory: 'permission' invalid: {}{}{}", permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');
            state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
            return;
        }

        auto tmem{state.process->NewHandle<type::KTransferMemory>(pointer, size, permission)};
        state.logger->Debug("svcCreateTransferMemory: Creating transfer memory at 0x{:X} - 0x{:X} (0x{:X} bytes) ({}{}{})", pointer, pointer + size, size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

        state.ctx->gpr.w0 = Result{};
        state.ctx->gpr.w1 = tmem.handle;
    }

    void CloseHandle(const DeviceState &state) {
        KHandle handle{static_cast<KHandle>(state.ctx->gpr.w0)};
        try {
            state.process->CloseHandle(handle);
            state.logger->Debug("svcCloseHandle: Closing handle: 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcCloseHandle: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ResetSignal(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        try {
            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KEvent:
                case type::KType::KProcess:
                    std::static_pointer_cast<type::KSyncObject>(object)->ResetSignal();
                    break;

                default: {
                    state.logger->Warn("svcResetSignal: 'handle' type invalid: 0x{:X} ({})", handle, object->objectType);
                    state.ctx->gpr.w0 = result::InvalidHandle;
                    return;
                }
            }

            state.logger->Debug("svcResetSignal: Resetting signal: 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcResetSignal: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
            return;
        }
    }

    void WaitSynchronization(const DeviceState &state) {
        constexpr u8 maxSyncHandles{0x40}; // The total amount of handles that can be passed to WaitSynchronization

        u32 numHandles{state.ctx->gpr.w2};
        if (numHandles > maxSyncHandles) {
            state.ctx->gpr.w0 = result::OutOfRange;
            return;
        }

        span waitHandles(reinterpret_cast<KHandle *>(state.ctx->gpr.x1), numHandles);
        std::vector<std::shared_ptr<type::KSyncObject>> objectTable;
        objectTable.reserve(numHandles);

        std::string handleString;
        for (const auto &handle : waitHandles) {
            if (Logger::LogLevel::Debug <= state.logger->configLevel)
                handleString += fmt::format("* 0x{:X}\n", handle);

            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KProcess:
                case type::KType::KThread:
                case type::KType::KEvent:
                case type::KType::KSession:
                    objectTable.push_back(std::static_pointer_cast<type::KSyncObject>(object));
                    break;

                default: {
                    state.logger->Debug("svcWaitSynchronization: An invalid handle was supplied: 0x{:X}", handle);
                    state.ctx->gpr.w0 = result::InvalidHandle;
                    return;
                }
            }
        }

        i64 timeout{static_cast<i64>(state.ctx->gpr.x3)};
        state.logger->Debug("svcWaitSynchronization: Waiting on handles:\n{}Timeout: {}ns", handleString, timeout);

        std::unique_lock lock(type::KSyncObject::syncObjectMutex);
        if (state.thread->cancelSync) {
            state.thread->cancelSync = false;
            state.ctx->gpr.w0 = result::Cancelled;
            return;
        }

        u32 index{};
        for (const auto &object : objectTable) {
            if (object->signalled) {
                state.logger->Debug("svcWaitSynchronization: Signalled handle: 0x{:X}", waitHandles[index]);
                state.ctx->gpr.w0 = Result{};
                state.ctx->gpr.w1 = index;
                return;
            }
            index++;
        }

        if (timeout == 0) {
            state.logger->Debug("svcWaitSynchronization: No handle is currently signalled");
            state.ctx->gpr.w0 = result::TimedOut;
            return;
        }

        auto priority{state.thread->priority.load()};
        for (const auto &object : objectTable)
            object->syncObjectWaiters.insert(std::upper_bound(object->syncObjectWaiters.begin(), object->syncObjectWaiters.end(), priority, type::KThread::IsHigherPriority), state.thread);

        state.thread->isCancellable = true;
        state.thread->wakeObject = nullptr;
        state.scheduler->RemoveThread();

        lock.unlock();
        if (timeout > 0)
            state.scheduler->TimedWaitSchedule(std::chrono::nanoseconds(timeout));
        else
            state.scheduler->WaitSchedule(false);
        lock.lock();

        state.thread->isCancellable = false;
        auto wakeObject{state.thread->wakeObject};

        u32 wakeIndex{};
        index = 0;
        for (const auto &object : objectTable) {
            if (object.get() == wakeObject)
                wakeIndex = index;

            auto it{std::find(object->syncObjectWaiters.begin(), object->syncObjectWaiters.end(), state.thread)};
            if (it != object->syncObjectWaiters.end())
                object->syncObjectWaiters.erase(it);
            else
                throw exception("svcWaitSynchronization: An object (0x{:X}) has been removed from the syncObjectWaiters queue incorrectly", waitHandles[index]);

            index++;
        }

        if (wakeObject) {
            state.logger->Debug("svcWaitSynchronization: Signalled handle: 0x{:X}", waitHandles[wakeIndex]);
            state.ctx->gpr.w0 = Result{};
            state.ctx->gpr.w1 = wakeIndex;
        } else if (state.thread->cancelSync) {
            state.thread->cancelSync = false;
            state.logger->Debug("svcWaitSynchronization: Wait has been cancelled");
            state.ctx->gpr.w0 = result::Cancelled;
        } else {
            state.logger->Debug("svcWaitSynchronization: Wait has timed out");
            state.ctx->gpr.w0 = result::TimedOut;
            lock.unlock();
            state.scheduler->InsertThread(state.thread);
            state.scheduler->WaitSchedule();
        }
    }

    void CancelSynchronization(const DeviceState &state) {
        try {
            std::unique_lock lock(type::KSyncObject::syncObjectMutex);
            auto thread{state.process->GetHandle<type::KThread>(state.ctx->gpr.w0)};
            thread->cancelSync = true;
            if (thread->isCancellable) {
                thread->isCancellable = false;
                state.scheduler->InsertThread(thread);
            }
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcCancelSynchronization: 'handle' invalid: 0x{:X}", static_cast<u32>(state.ctx->gpr.w0));
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ArbitrateLock(const DeviceState &state) {
        auto pointer{reinterpret_cast<u32 *>(state.ctx->gpr.x1)};
        if (!util::WordAligned(pointer)) {
            state.logger->Warn("svcArbitrateLock: 'pointer' not word aligned: 0x{:X}", pointer);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        state.logger->Debug("svcArbitrateLock: Locking mutex at 0x{:X}", pointer);

        KHandle ownerHandle{state.ctx->gpr.w0};
        KHandle requesterHandle{state.ctx->gpr.w2};
        auto result{state.process->MutexLock(pointer, ownerHandle, requesterHandle)};
        if (result == Result{})
            state.logger->Debug("svcArbitrateLock: Locked mutex at 0x{:X}", pointer);
        else if (result == result::InvalidHandle)
            state.logger->Warn("svcArbitrateLock: 'ownerHandle' invalid: 0x{:X}", ownerHandle);
        else if (result == result::InvalidCurrentMemory)
            state.logger->Debug("svcArbitrateLock: Owner handle did not match current owner for mutex or didn't have waiter flag at 0x{:X}", pointer);

        state.ctx->gpr.w0 = result;
    }

    void ArbitrateUnlock(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        if (!util::WordAligned(mutex)) {
            state.logger->Warn("svcArbitrateUnlock: 'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        state.logger->Debug("svcArbitrateUnlock: Unlocking mutex at 0x{:X}", mutex);
        state.process->MutexUnlock(mutex);
        state.logger->Debug("svcArbitrateUnlock: Unlocked mutex at 0x{:X}", mutex);

        state.ctx->gpr.w0 = Result{};
    }

    void WaitProcessWideKeyAtomic(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        if (!util::WordAligned(mutex)) {
            state.logger->Warn("svcWaitProcessWideKeyAtomic: 'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        auto conditional{reinterpret_cast<u32 *>(state.ctx->gpr.x1)};
        KHandle requesterHandle{state.ctx->gpr.w2};

        i64 timeout{static_cast<i64>(state.ctx->gpr.x3)};
        state.logger->Debug("svcWaitProcessWideKeyAtomic: Mutex: 0x{:X}, Conditional-Variable: 0x{:X}, Timeout: {}ns", mutex, conditional, timeout);

        auto result{state.process->ConditionalVariableWait(conditional, mutex, requesterHandle, timeout)};
        if (result == Result{})
            state.logger->Debug("svcWaitProcessWideKeyAtomic: Waited for conditional variable (0x{:X}) and reacquired mutex", conditional);
        else if (result == result::TimedOut)
            state.logger->Debug("svcWaitProcessWideKeyAtomic: Wait has timed out ({}ns) for 0x{:X}", timeout, conditional);
        state.ctx->gpr.w0 = result;
    }

    void SignalProcessWideKey(const DeviceState &state) {
        auto key{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        KHandle count{state.ctx->gpr.w1};

        state.logger->Debug("svcSignalProcessWideKey: Signalling Conditional-Variable at 0x{:X} for {}", key, count);
        state.process->ConditionalVariableSignal(key, count);
        state.ctx->gpr.w0 = Result{};
    }

    void GetSystemTick(const DeviceState &state) {
        u64 tick;
        asm("STR X1, [SP, #-16]!\n\t"
            "MRS %0, CNTVCT_EL0\n\t"
            "MOV X1, #0xF800\n\t"
            "MOVK X1, #0x124, lsl #16\n\t"
            "MUL %0, %0, X1\n\t"
            "MRS X1, CNTFRQ_EL0\n\t"
            "UDIV %0, %0, X1\n\t"
            "LDR X1, [SP], #16" : "=r"(tick));
        state.ctx->gpr.x0 = tick;
    }

    void ConnectToNamedPort(const DeviceState &state) {
        constexpr u8 portSize = 0x8; //!< The size of a port name string
        std::string_view port(span(reinterpret_cast<char *>(state.ctx->gpr.x1), portSize).as_string(true));

        KHandle handle{};
        if (port.compare("sm:") >= 0) {
            handle = state.process->NewHandle<type::KSession>(std::static_pointer_cast<service::BaseService>(state.os->serviceManager.smUserInterface)).handle;
        } else {
            state.logger->Warn("svcConnectToNamedPort: Connecting to invalid port: '{}'", port);
            state.ctx->gpr.w0 = result::NotFound;
            return;
        }

        state.logger->Debug("svcConnectToNamedPort: Connecting to port '{}' at 0x{:X}", port, handle);

        state.ctx->gpr.w1 = handle;
        state.ctx->gpr.w0 = Result{};
    }

    void SendSyncRequest(const DeviceState &state) {
        SchedulerScopedLock schedulerLock(state);
        state.os->serviceManager.SyncRequestHandler(static_cast<KHandle>(state.ctx->gpr.x0));
        state.ctx->gpr.w0 = Result{};
    }

    void GetThreadId(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w1};
        size_t tid{state.process->GetHandle<type::KThread>(handle)->id};

        state.logger->Debug("svcGetThreadId: Handle: 0x{:X}, TID: {}", handle, tid);

        state.ctx->gpr.x1 = tid;
        state.ctx->gpr.w0 = Result{};
    }

    void Break(const DeviceState &state) {
        auto reason{state.ctx->gpr.x0};
        if (reason & (1ULL << 31)) {
            state.logger->Error("svcBreak: Debugger is being engaged ({})", reason);
            __builtin_trap();
        } else {
            state.logger->Error("svcBreak: Stack Trace ({}){}", reason, state.loader->GetStackTrace());
            if (state.thread->id)
                state.process->Kill(false);
            std::longjmp(state.thread->originalCtx, true);
        }
    }

    void OutputDebugString(const DeviceState &state) {
        auto string{span(reinterpret_cast<u8 *>(state.ctx->gpr.x0), state.ctx->gpr.x1).as_string()};

        if (string.back() == '\n')
            string.remove_suffix(1);

        state.logger->Info("svcOutputDebugString: {}", string);
        state.ctx->gpr.w0 = Result{};
    }

    void GetInfo(const DeviceState &state) {
        enum class InfoState : u32 {
            // 1.0.0+
            AllowedCpuIdBitmask = 0,
            AllowedThreadPriorityMask = 1,
            AliasRegionBaseAddr = 2,
            AliasRegionSize = 3,
            HeapRegionBaseAddr = 4,
            HeapRegionSize = 5,
            TotalMemoryAvailable = 6,
            TotalMemoryUsage = 7,
            IsCurrentProcessBeingDebugged = 8,
            ResourceLimit = 9,
            IdleTickCount = 10,
            RandomEntropy = 11,
            // 2.0.0+
            AddressSpaceBaseAddr = 12,
            AddressSpaceSize = 13,
            StackRegionBaseAddr = 14,
            StackRegionSize = 15,
            // 3.0.0+
            TotalSystemResourceAvailable = 16,
            TotalSystemResourceUsage = 17,
            ProgramId = 18,
            // 4.0.0+
            PrivilegedProcessId = 19,
            // 5.0.0+
            UserExceptionContextAddr = 20,
            // 6.0.0+
            TotalMemoryAvailableWithoutSystemResource = 21,
            TotalMemoryUsageWithoutSystemResource = 22,
        };

        InfoState info{static_cast<u32>(state.ctx->gpr.w1)};
        KHandle handle{state.ctx->gpr.w2};
        u64 id1{state.ctx->gpr.x3};

        constexpr u64 totalPhysicalMemory{0xF8000000}; // ~4 GB of RAM

        u64 out{};
        switch (info) {
            case InfoState::IsCurrentProcessBeingDebugged:
            case InfoState::PrivilegedProcessId:
                break;

            case InfoState::AllowedCpuIdBitmask:
                out = state.process->npdm.threadInfo.coreMask.to_ullong();
                break;

            case InfoState::AllowedThreadPriorityMask:
                out = state.process->npdm.threadInfo.priority.Mask();
                break;

            case InfoState::AliasRegionBaseAddr:
                out = state.process->memory.alias.address;
                break;

            case InfoState::AliasRegionSize:
                out = state.process->memory.alias.size;
                break;

            case InfoState::HeapRegionBaseAddr:
                out = state.process->memory.heap.address;
                break;

            case InfoState::HeapRegionSize:
                out = state.process->memory.heap.size;
                break;

            case InfoState::TotalMemoryAvailable:
                out = std::min(totalPhysicalMemory, state.process->memory.heap.size);
                break;

            case InfoState::TotalMemoryUsage:
                out = state.process->memory.GetUserMemoryUsage() + state.process->memory.GetSystemResourceUsage();
                break;

            case InfoState::RandomEntropy:
                out = util::GetTimeTicks();
                break;

            case InfoState::AddressSpaceBaseAddr:
                out = state.process->memory.base.address;
                break;

            case InfoState::AddressSpaceSize:
                out = state.process->memory.base.size;
                break;

            case InfoState::StackRegionBaseAddr:
                out = state.process->memory.stack.address;
                break;

            case InfoState::StackRegionSize:
                out = state.process->memory.stack.size;
                break;

            case InfoState::TotalSystemResourceAvailable:
                out = state.process->npdm.meta.systemResourceSize;
                break;

            case InfoState::TotalSystemResourceUsage:
                // A very rough approximation of what this should be on the Switch, the amount of memory allocated for storing the memory blocks (https://switchbrew.org/wiki/Kernel_objects#KMemoryBlockManager)
                out = state.process->memory.GetSystemResourceUsage();
                break;

            case InfoState::ProgramId:
                out = state.process->npdm.aci0.programId;
                break;

            case InfoState::TotalMemoryAvailableWithoutSystemResource:
                out = std::min(totalPhysicalMemory, state.process->memory.heap.size) - state.process->npdm.meta.systemResourceSize;
                break;

            case InfoState::TotalMemoryUsageWithoutSystemResource:
                out = state.process->memory.GetUserMemoryUsage();
                break;

            case InfoState::UserExceptionContextAddr:
                out = reinterpret_cast<u64>(state.process->tlsExceptionContext);
                break;

            default:
                state.logger->Warn("svcGetInfo: Unimplemented case ID0: {}, ID1: {}", static_cast<u32>(info), id1);
                state.ctx->gpr.w0 = result::InvalidEnumValue;
                return;
        }

        state.logger->Debug("svcGetInfo: ID0: {}, ID1: {}, Out: 0x{:X}", static_cast<u32>(info), id1, out);

        state.ctx->gpr.x1 = out;
        state.ctx->gpr.w0 = Result{};
    }

    void MapPhysicalMemory(const DeviceState &state) {
        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        size_t size{state.ctx->gpr.x1};

        if (!util::PageAligned(pointer)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        if (!size || !util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            return;
        }

        if (!state.process->memory.alias.IsInside(pointer) || !state.process->memory.alias.IsInside(pointer + size)) {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            return;
        }

        state.process->NewHandle<type::KPrivateMemory>(pointer, size, memory::Permission{true, true, false}, memory::states::Heap);

        state.ctx->gpr.w0 = Result{};
    }

    void UnmapPhysicalMemory(const DeviceState &state) {
        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        size_t size{state.ctx->gpr.x1};

        if (!util::PageAligned(pointer)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        if (!size || !util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            return;
        }

        if (!state.process->memory.alias.IsInside(pointer) || !state.process->memory.alias.IsInside(pointer + size)) {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            return;
        }

        auto end{pointer + size};
        while (pointer < end) {
            auto memory{state.process->GetMemoryObject(pointer)};
            if (memory) {
                auto item{static_pointer_cast<type::KPrivateMemory>(memory->item)};
                auto initialSize{item->size};
                if (item->memState == memory::states::Heap) {
                    if (item->ptr >= pointer) {
                        if (item->size <= size) {
                            item->Resize(0);
                            state.process->CloseHandle(memory->handle);
                        } else {
                            item->Remap(pointer + size, item->size - (size + (item->ptr - pointer)));
                        }
                    } else if (item->ptr < pointer) {
                        item->Resize(pointer - item->ptr);
                    }
                }
                pointer += initialSize;
                size -= initialSize;
            } else {
                auto block{*state.process->memory.Get(pointer)};
                pointer += block.size;
                size -= block.size;
            }
        }

        state.ctx->gpr.w0 = Result{};
    }
}
