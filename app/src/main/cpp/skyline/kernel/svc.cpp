// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <nce.h>
#include <kernel/types/KProcess.h>
#include <common/trace.h>
#include <vfs/npdm.h>
#include "results.h"
#include "svc.h"

namespace skyline::kernel::svc {
    void SetHeapSize(const DeviceState &state) {
        u64 size{state.ctx->gpr.w1};

        if (!util::IsAligned(size, 0x200000)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.ctx->gpr.x1 = 0;

            Logger::Warn("'size' not divisible by 2MB: 0x{:X}", size);
            return;
        } else if (state.process->memory.heap.size() < size) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.ctx->gpr.x1 = 0;

            Logger::Warn("'size' exceeded size of heap region: 0x{:X}", size);
            return;
        }

        size_t heapCurrSize{state.process->memory.setHeapSize};
        u8 *heapBaseAddr{state.process->memory.heap.data()};

        if (heapCurrSize < size)
            state.process->memory.MapHeapMemory(span<u8>{heapBaseAddr + heapCurrSize, size - heapCurrSize});
        else if (size < heapCurrSize)
            state.process->memory.UnmapMemory(span<u8>{heapBaseAddr + size, heapCurrSize - size});

        state.process->memory.setHeapSize = size;

        state.ctx->gpr.w0 = Result{};
        state.ctx->gpr.x1 = reinterpret_cast<u64>(heapBaseAddr);

        Logger::Debug("Heap size changed to 0x{:X} bytes (0x{:X} - 0x{:X})", size, heapBaseAddr, heapBaseAddr + size);
    }

    void SetMemoryPermission(const DeviceState &state) {
        u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        if (!util::IsPageAligned(address)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("'address' not page aligned: 0x{:X}", address);
            return;
        }

        u64 size{state.ctx->gpr.x1};
        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidCurrentMemory;
            Logger::Warn("Invalid address and size combination: 'address': 0x{:X}, 'size': 0x{:X} ", address, size);
            return;
        }

        memory::Permission newPermission(static_cast<u8>(state.ctx->gpr.w2));
        if ((!newPermission.r && newPermission.w) || newPermission.x) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
            Logger::Warn("'permission' invalid: {}{}{}", newPermission.r ? 'R' : '-', newPermission.w ? 'W' : '-', newPermission.x ? 'X' : '-');
            return;
        }

        auto chunk{state.process->memory.GetChunk(address).value()};
        if (!chunk.second.state.permissionChangeAllowed) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidState;
            Logger::Warn("Permission change not allowed for chunk at: 0x{:X}, state: 0x{:X}", chunk.first, chunk.second.state.value);
            return;
        }

        state.process->memory.SetChunkPermission(span<u8>(address, size), newPermission);

        Logger::Debug("Set permission to {}{}{} at 0x{:X} - 0x{:X} (0x{:X} bytes)", newPermission.r ? 'R' : '-', newPermission.w ? 'W' : '-', newPermission.x ? 'X' : '-', address, address + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void SetMemoryAttribute(const DeviceState &state) {
        u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        if (!util::IsPageAligned(address)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("'address' not page aligned: 0x{:X}", address);
            return;
        }

        u64 size{state.ctx->gpr.x1};
        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidCurrentMemory;
            Logger::Warn("Invalid address and size combination: 'address': 0x{:X}, 'size': 0x{:X} ", address, size);
            return;
        }

        memory::MemoryAttribute mask{.value = state.ctx->gpr.w2};
        memory::MemoryAttribute value{.value = state.ctx->gpr.w3};

        auto maskedValue{mask.value | value.value};
        if (maskedValue != mask.value || !mask.isUncached || mask.isDeviceShared || mask.isBorrowed || mask.isIpcLocked) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidCombination;
            Logger::Warn("'mask' invalid: 0x{:X}, 0x{:X}", mask.value, value.value);
            return;
        }

        auto chunk{state.process->memory.GetChunk(address).value()};

        // We only check the first found chunk for whatever reason.
        if (!chunk.second.state.attributeChangeAllowed) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidState;
            Logger::Warn("Attribute change not allowed for chunk: 0x{:X}", chunk.first);
            return;
        }

        state.process->memory.SetCPUCachingOnChunks(span<u8>{address, size}, value.isUncached);

        Logger::Debug("Set CPU caching to {} at 0x{:X} - 0x{:X} (0x{:X} bytes)", static_cast<bool>(value.isUncached), address, address + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void MapMemory(const DeviceState &state) {
        u8 *destination{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        u8 *source{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        size_t size{state.ctx->gpr.x2};

        if (!util::IsPageAligned(destination) || !util::IsPageAligned(source)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("Addresses not page aligned: 'source': 0x{:X}, 'destination': 0x{:X}, 'size': 0x{:X} bytes", source, destination, size);
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (destination >= (destination + size) || !state.process->memory.AddressSpaceContains(span<u8>{destination, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidCurrentMemory;
            Logger::Warn("Invalid address and size combination: 'destination': 0x{:X}, 'size': 0x{:X} bytes", destination, size);
            return;
        }

        if (source >= (source + size) || !state.process->memory.AddressSpaceContains(span<u8>{source, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidCurrentMemory;
            Logger::Warn("Invalid address and size combination: 'source': 0x{:X}, 'size': 0x{:X} bytes", source, size);
            return;
        }

        if (!state.process->memory.stack.contains(span<u8>{destination, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            Logger::Warn("Destination not within stack region: 'source': 0x{:X}, 'destination': 0x{:X}, 'size': 0x{:X} bytes", source, destination, size);
            return;
        }

        auto chunk{state.process->memory.GetChunk(source)};
        if (!chunk->second.state.mapAllowed) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidState;
            Logger::Warn("Source doesn't allow usage of svcMapMemory: 'source': 0x{:X}, 'size': 0x{:X}, MemoryState: 0x{:X}", source, size, chunk->second.state.value);
            return;
        }

        state.process->memory.MapStackMemory(span<u8>{destination, size});
        std::memcpy(destination, source, size);

        state.process->memory.SetChunkPermission(span<u8>{source, size}, {false, false, false});
        state.process->memory.SetLockOnChunks(span<u8>{source, size}, true);

        Logger::Debug("Mapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", source, source + size, destination, destination + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void UnmapMemory(const DeviceState &state) {
        u8 *destination{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        u8 *source{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        size_t size{state.ctx->gpr.x2};

        if (!util::IsPageAligned(destination) || !util::IsPageAligned(source)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("Addresses not page aligned: 'source': 0x{:X}, 'destination': 0x{:X}, 'size': {} bytes", source, destination, size);
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (!state.process->memory.stack.contains(span<u8>{destination, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            Logger::Warn("Source not within stack region: 'source': 0x{:X}, 'destination': 0x{:X}, 'size': 0x{:X} bytes", source, destination, size);
            return;
        }

        auto dstChunk{state.process->memory.GetChunk(destination).value()};
        while (dstChunk.second.state.value == memory::states::Unmapped)
            dstChunk = state.process->memory.GetChunk(dstChunk.first + dstChunk.second.size).value();

        if ((destination + size) > dstChunk.first) [[likely]] {
            state.process->memory.SetChunkPermission(span<u8>{source + (dstChunk.first - destination), dstChunk.second.size}, dstChunk.second.permission);
            state.process->memory.SetLockOnChunks(span<u8>{source + (dstChunk.first - destination), dstChunk.second.size}, false);

            std::memcpy(source + (dstChunk.first - destination), dstChunk.first, dstChunk.second.size);

            state.process->memory.UnmapMemory(span<u8>{destination, size});
        }

        Logger::Debug("Unmapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", destination, destination + size, source, source + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void QueryMemory(const DeviceState &state) {
        memory::MemoryInfo memInfo{};

        u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x2)};
        auto chunk{state.process->memory.GetChunk(address)};

        if (chunk) {
            memInfo = {
                .address = reinterpret_cast<u64>(chunk->first),
                .size = chunk->second.size,
                .type = static_cast<u32>(chunk->second.state.type),
                .attributes = chunk->second.attributes.value,
                .permissions = static_cast<u32>(chunk->second.permission.Get()),
                .deviceRefCount = 0,
                .ipcRefCount = 0,
            };

            Logger::Debug("Address: 0x{:X}, Region Start: 0x{:X}, Size: 0x{:X}, Type: 0x{:X}, Attributes: 0x{:X}, Permissions: {}{}{}", address, memInfo.address, memInfo.size, memInfo.type, memInfo.attributes, chunk->second.permission.r ? 'R' : '-', chunk->second.permission.w ? 'W' : '-', chunk->second.permission.x ? 'X' : '-');
        } else {
            u64 addressSpaceEnd{reinterpret_cast<u64>(state.process->memory.addressSpace.end().base())};

            memInfo = {
                .address = addressSpaceEnd,
                .size = 0 - addressSpaceEnd,
                .type = static_cast<u32>(memory::MemoryType::Reserved),
            };

            Logger::Debug("Trying to query memory outside of the application's address space: 0x{:X}", address);
        }

        *reinterpret_cast<memory::MemoryInfo *>(state.ctx->gpr.x0) = memInfo;
        // The page info, which is always 0
        state.ctx->gpr.x1 = 0;

        state.ctx->gpr.w0 = Result{};
    }

    void ExitProcess(const DeviceState &state) {
        Logger::Debug("Exiting process");
        throw nce::NCE::ExitException(true);
    }

    constexpr i32 IdealCoreDontCare{-1};
    constexpr i32 IdealCoreUseProcessValue{-2};
    constexpr i32 IdealCoreNoUpdate{-3};

    void CreateThread(const DeviceState &state) {
        auto entry{reinterpret_cast<void *>(state.ctx->gpr.x1)};
        auto entryArgument{state.ctx->gpr.x2};
        auto stackTop{reinterpret_cast<u8 *>(state.ctx->gpr.x3)};
        auto priority{static_cast<i8>(state.ctx->gpr.w4)};
        auto idealCore{static_cast<i32>(state.ctx->gpr.w5)};

        idealCore = (idealCore == IdealCoreUseProcessValue) ? static_cast<i32>(state.process->npdm.meta.idealCore) : idealCore;
        if (idealCore < 0 || idealCore >= constant::CoreCount) {
            state.ctx->gpr.w0 = result::InvalidCoreId;
            Logger::Warn("'idealCore' invalid: {}", idealCore);
            return;
        }

        if (!state.process->npdm.threadInfo.priority.Valid(priority)) {
            state.ctx->gpr.w0 = result::InvalidPriority;
            Logger::Warn("'priority' invalid: {}", priority);
            return;
        }

        auto thread{state.process->CreateThread(entry, entryArgument, stackTop, priority, static_cast<u8>(idealCore))};
        if (thread) {
            Logger::Debug("Created thread #{} with handle 0x{:X} (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, Ideal Core: {})", thread->id, thread->handle, entry, entryArgument, stackTop, priority, idealCore);

            state.ctx->gpr.w1 = thread->handle;
            state.ctx->gpr.w0 = Result{};
        } else {
            Logger::Debug("Cannot create thread (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, Ideal Core: {})", entry, entryArgument, stackTop, priority, idealCore);
            state.ctx->gpr.w1 = 0;
            state.ctx->gpr.w0 = result::OutOfResource;
        }
    }

    void StartThread(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            Logger::Debug("Starting thread #{}: 0x{:X}", thread->id, handle);
            thread->Start();
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ExitThread(const DeviceState &state) {
        Logger::Debug("Exiting current thread");
        throw nce::NCE::ExitException(false);
    }

    void SleepThread(const DeviceState &state) {
        constexpr i64 yieldWithoutCoreMigration{0};
        constexpr i64 yieldWithCoreMigration{-1};
        constexpr i64 yieldToAnyThread{-2};

        i64 in{static_cast<i64>(state.ctx->gpr.x0)};
        if (in > 0) {
            Logger::Debug("Sleeping for {}ns", in);
            TRACE_EVENT("kernel", "SleepThread", "duration", in);

            struct timespec spec{
                .tv_sec = static_cast<time_t>(in / 1000000000),
                .tv_nsec = static_cast<long>(in % 1000000000),
            };

            SchedulerScopedLock schedulerLock(state);
            nanosleep(&spec, nullptr);
        } else {
            switch (in) {
                case yieldWithCoreMigration: {
                    Logger::Debug("Waking any appropriate parked threads and yielding");
                    TRACE_EVENT("kernel", "YieldWithCoreMigration");
                    state.scheduler->WakeParkedThread();
                    state.scheduler->Rotate();
                    state.scheduler->WaitSchedule();
                    break;
                }

                case yieldWithoutCoreMigration: {
                    Logger::Debug("Cooperative yield");
                    TRACE_EVENT("kernel", "YieldWithoutCoreMigration");
                    state.scheduler->Rotate();
                    state.scheduler->WaitSchedule();
                    break;
                }

                case yieldToAnyThread: {
                    Logger::Debug("Parking current thread");
                    TRACE_EVENT("kernel", "YieldToAnyThread");
                    state.scheduler->ParkThread();
                    state.scheduler->WaitSchedule(false);
                    break;
                }

                default:
                    break;
            }
        }
    }

    void GetThreadPriority(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w1};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            i8 priority{thread->priority};
            Logger::Debug("Retrieving thread #{}'s priority: {}", thread->id, priority);

            state.ctx->gpr.w1 = static_cast<u32>(priority);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void SetThreadPriority(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        i8 priority{static_cast<i8>(state.ctx->gpr.w1)};
        if (!state.process->npdm.threadInfo.priority.Valid(priority)) {
            Logger::Warn("'priority' invalid: 0x{:X}", priority);
            state.ctx->gpr.w0 = result::InvalidPriority;
            return;
        }
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            Logger::Debug("Setting thread #{}'s priority to {}", thread->id, priority);
            if (thread->priority != priority) {
                thread->basePriority = priority;
                i8 newPriority{};
                do {
                    // Try to CAS the priority of the thread with its new base priority
                    // If the new priority is equivalent to the current priority then we don't need to CAS
                    newPriority = thread->priority.load();
                    newPriority = std::min(newPriority, priority);
                } while (newPriority != priority && !thread->priority.compare_exchange_strong(newPriority, priority));
                state.scheduler->UpdatePriority(thread);
                thread->UpdatePriorityInheritance();
            }
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void GetThreadCoreMask(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w2};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            auto idealCore{thread->idealCore};
            auto affinityMask{thread->affinityMask};
            Logger::Debug("Getting thread #{}'s Ideal Core ({}) + Affinity Mask ({})", thread->id, idealCore, affinityMask);

            state.ctx->gpr.x2 = affinityMask.to_ullong();
            state.ctx->gpr.w1 = static_cast<u32>(idealCore);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
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
                affinityMask.reset().set(static_cast<size_t>(idealCore));
            } else if (idealCore == IdealCoreNoUpdate) {
                idealCore = thread->idealCore;
            } else if (idealCore == IdealCoreDontCare) {
                idealCore = std::countr_zero(affinityMask.to_ullong()); // The first enabled core in the affinity mask
            }

            auto processMask{state.process->npdm.threadInfo.coreMask};
            if ((processMask | affinityMask) != processMask) {
                Logger::Warn("'affinityMask' invalid: {} (Process Mask: {})", affinityMask, processMask);
                state.ctx->gpr.w0 = result::InvalidCoreId;
                return;
            }

            if (affinityMask.none() || !affinityMask.test(static_cast<size_t>(idealCore))) {
                Logger::Warn("'affinityMask' invalid: {} (Ideal Core: {})", affinityMask, idealCore);
                state.ctx->gpr.w0 = result::InvalidCombination;
                return;
            }

            Logger::Debug("Setting thread #{}'s Ideal Core ({}) + Affinity Mask ({})", thread->id, idealCore, affinityMask);

            std::scoped_lock guard{thread->coreMigrationMutex};
            thread->idealCore = static_cast<u8>(idealCore);
            thread->affinityMask = affinityMask;

            if (!affinityMask.test(static_cast<size_t>(thread->coreId)) && thread->coreId != constant::ParkedCoreId) {
                Logger::Debug("Migrating thread #{} to Ideal Core C{} -> C{}", thread->id, thread->coreId, idealCore);

                if (thread == state.thread) {
                    state.scheduler->RemoveThread();
                    thread->coreId = static_cast<u8>(idealCore);
                    state.scheduler->InsertThread(state.thread);
                    state.scheduler->WaitSchedule();
                } else if (!thread->running) {
                    thread->coreId = static_cast<u8>(idealCore);
                } else {
                    state.scheduler->UpdateCore(thread);
                }
            }

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void GetCurrentProcessorNumber(const DeviceState &state) {
        std::scoped_lock guard{state.thread->coreMigrationMutex};
        u8 coreId{state.thread->coreId};
        Logger::Debug("C{}", coreId);
        state.ctx->gpr.w0 = coreId;
    }

    void ClearEvent(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        TRACE_EVENT_FMT("kernel", "ClearEvent 0x{:X}", handle);
        try {
            std::static_pointer_cast<type::KEvent>(state.process->GetHandle(handle))->ResetSignal();
            Logger::Debug("Clearing 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
            return;
        }
    }

    void MapSharedMemory(const DeviceState &state) {
        try {
            KHandle handle{state.ctx->gpr.w0};
            auto object{state.process->GetHandle<type::KSharedMemory>(handle)};
            u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};

            if (!util::IsPageAligned(address)) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidAddress;
                Logger::Warn("'address' not page aligned: 0x{:X}", address);
                return;
            }

            size_t size{state.ctx->gpr.x2};
            if (!size || !util::IsPageAligned(size)) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidSize;
                Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
                return;
            }

            if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidCurrentMemory;
                Logger::Warn("Invalid address and size combination: 'address': 0x{:X}, 'size': 0x{:X}", address, size);
                return;
            }

            memory::Permission permission(static_cast<u8>(state.ctx->gpr.w3));
            if ((!permission.r && !permission.w && !permission.x) || (permission.w && !permission.r) || permission.x) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
                Logger::Warn("'permission' invalid: {}{}{}", permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');
                return;
            }

            Logger::Debug("Mapping shared memory (0x{:X}) at 0x{:X} - 0x{:X} (0x{:X} bytes), with permissions: ({}{}{})", handle, address, address + size, size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

            object->Map(span<u8>{address, size}, permission);
            state.process->memory.AddRef(object);

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", static_cast<u32>(state.ctx->gpr.w0));
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void UnmapSharedMemory(const DeviceState &state) {
        try {
            KHandle handle{state.ctx->gpr.w0};
            auto object{state.process->GetHandle<type::KSharedMemory>(handle)};
            u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};

            if (!util::IsPageAligned(address)) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidAddress;
                Logger::Warn("'address' not page aligned: 0x{:X}", address);
                return;
            }

            size_t size{state.ctx->gpr.x2};
            if (!size || !util::IsPageAligned(size)) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidSize;
                Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
                return;
            }

            if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
                state.ctx->gpr.w0 = result::InvalidCurrentMemory;
                Logger::Warn("Invalid address and size combination: 'address': 0x{:X}, 'size': 0x{:X}", address, size);
                return;
            }

            Logger::Debug("Unmapping shared memory (0x{:X}) at 0x{:X} - 0x{:X} (0x{:X} bytes)", handle, address, address + size, size);

            object->Unmap(span<u8>{address, size});
            state.process->memory.RemoveRef(object);

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", static_cast<u32>(state.ctx->gpr.w0));
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void CreateTransferMemory(const DeviceState &state) {
        u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        if (!util::IsPageAligned(address)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("'address' not page aligned: 0x{:X}", address);
            return;
        }

        size_t size{state.ctx->gpr.x2};
        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidCurrentMemory;
            Logger::Warn("Invalid address and size combination: 'address': 0x{:X}, 'size': 0x{:X}", address, size);
            return;
        }

        memory::Permission permission(static_cast<u8>(state.ctx->gpr.w3));
        if ((permission.w && !permission.r) || permission.x) [[unlikely]] {
            Logger::Warn("'permission' invalid: {}{}{}", permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');
            state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
            return;
        }

        auto tmem{state.process->NewHandle<kernel::type::KTransferMemory>(size)};
        if (!tmem.item->Map(span<u8>{address, size}, permission)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidState;
            return;
        }

        Logger::Debug("Creating transfer memory (0x{:X}) at 0x{:X} - 0x{:X} (0x{:X} bytes) ({}{}{})", tmem.handle, address, address + size, size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

        state.ctx->gpr.w0 = Result{};
        state.ctx->gpr.w1 = tmem.handle;
    }

    void CloseHandle(const DeviceState &state) {
        KHandle handle{static_cast<KHandle>(state.ctx->gpr.w0)};
        try {
            state.process->CloseHandle(handle);
            Logger::Debug("Closing 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ResetSignal(const DeviceState &state) {
        KHandle handle{state.ctx->gpr.w0};
        TRACE_EVENT_FMT("kernel", "ResetSignal 0x{:X}", handle);
        try {
            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KEvent:
                case type::KType::KProcess:
                    state.ctx->gpr.w0 = std::static_pointer_cast<type::KSyncObject>(object)->ResetSignal() ? Result{} : result::InvalidState;
                    break;

                default: {
                    Logger::Warn("'handle' type invalid: 0x{:X} ({})", handle, object->objectType);
                    state.ctx->gpr.w0 = result::InvalidHandle;
                    return;
                }
            }

            Logger::Debug("Resetting 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
            return;
        }
    }

    void WaitSynchronization(const DeviceState &state) {
        constexpr u8 MaxSyncHandles{0x40}; // The total amount of handles that can be passed to WaitSynchronization

        u32 numHandles{state.ctx->gpr.w2};
        if (numHandles > MaxSyncHandles) {
            state.ctx->gpr.w0 = result::OutOfRange;
            return;
        }

        span waitHandles(reinterpret_cast<KHandle *>(state.ctx->gpr.x1), numHandles);
        std::vector<std::shared_ptr<type::KSyncObject>> objectTable;
        objectTable.reserve(numHandles);

        for (const auto &handle : waitHandles) {
            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KProcess:
                case type::KType::KThread:
                case type::KType::KEvent:
                case type::KType::KSession:
                    objectTable.push_back(std::static_pointer_cast<type::KSyncObject>(object));
                    break;

                default: {
                    Logger::Debug("An invalid handle was supplied: 0x{:X}", handle);
                    state.ctx->gpr.w0 = result::InvalidHandle;
                    return;
                }
            }
        }

        i64 timeout{static_cast<i64>(state.ctx->gpr.x3)};
        if (waitHandles.size() == 1) {
            Logger::Debug("Waiting on 0x{:X} for {}ns", waitHandles[0], timeout);
        } else if (Logger::LogLevel::Debug <= Logger::configLevel) {
            std::string handleString;
            for (const auto &handle : waitHandles)
                handleString += fmt::format("* 0x{:X}\n", handle);
            Logger::Debug("Waiting on handles:\n{}Timeout: {}ns", handleString, timeout);
        }

        TRACE_EVENT_FMT("kernel", waitHandles.size() == 1 ? "WaitSynchronization 0x{:X}" : "WaitSynchronizationMultiple 0x{:X}", waitHandles[0]);

        std::unique_lock lock(type::KSyncObject::syncObjectMutex);
        if (state.thread->cancelSync) {
            state.thread->cancelSync = false;
            state.ctx->gpr.w0 = result::Cancelled;
            return;
        }

        u32 index{};
        for (const auto &object : objectTable) {
            if (object->signalled) {
                Logger::Debug("Signalled 0x{:X}", waitHandles[index]);
                state.ctx->gpr.w0 = Result{};
                state.ctx->gpr.w1 = index;
                return;
            }
            index++;
        }

        if (timeout == 0) {
            Logger::Debug("No handle is currently signalled");
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
            Logger::Debug("Signalled 0x{:X}", waitHandles[wakeIndex]);
            state.ctx->gpr.w0 = Result{};
            state.ctx->gpr.w1 = wakeIndex;
        } else if (state.thread->cancelSync) {
            state.thread->cancelSync = false;
            Logger::Debug("Wait has been cancelled");
            state.ctx->gpr.w0 = result::Cancelled;
        } else {
            Logger::Debug("Wait has timed out");
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
            Logger::Debug("Cancelling Synchronization {}", thread->id);
            thread->cancelSync = true;
            if (thread->isCancellable) {
                thread->isCancellable = false;
                state.scheduler->InsertThread(thread);
            }
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", static_cast<u32>(state.ctx->gpr.w0));
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ArbitrateLock(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32 *>(state.ctx->gpr.x1)};
        if (!util::IsWordAligned(mutex)) {
            Logger::Warn("'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        Logger::Debug("Locking 0x{:X}", mutex);

        KHandle ownerHandle{state.ctx->gpr.w0};
        KHandle requesterHandle{state.ctx->gpr.w2};
        auto result{state.process->MutexLock(state.thread, mutex, ownerHandle, requesterHandle)};
        if (result == Result{})
            Logger::Debug("Locked 0x{:X}", mutex);
        else if (result == result::InvalidCurrentMemory)
            result = Result{}; // If the mutex value isn't expected then it's still successful
        else if (result == result::InvalidHandle)
            Logger::Warn("'ownerHandle' invalid: 0x{:X} (0x{:X})", ownerHandle, mutex);

        state.ctx->gpr.w0 = result;
    }

    void ArbitrateUnlock(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        if (!util::IsWordAligned(mutex)) {
            Logger::Warn("'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        Logger::Debug("Unlocking 0x{:X}", mutex);
        state.process->MutexUnlock(mutex);
        Logger::Debug("Unlocked 0x{:X}", mutex);

        state.ctx->gpr.w0 = Result{};
    }

    void WaitProcessWideKeyAtomic(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        if (!util::IsWordAligned(mutex)) {
            Logger::Warn("'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        auto conditional{reinterpret_cast<u32 *>(state.ctx->gpr.x1)};
        KHandle requesterHandle{state.ctx->gpr.w2};

        i64 timeout{static_cast<i64>(state.ctx->gpr.x3)};
        Logger::Debug("Waiting on 0x{:X} with 0x{:X} for {}ns", conditional, mutex, timeout);

        auto result{state.process->ConditionVariableWait(conditional, mutex, requesterHandle, timeout)};
        if (result == Result{})
            Logger::Debug("Waited for 0x{:X} and reacquired 0x{:X}", conditional, mutex);
        else if (result == result::TimedOut)
            Logger::Debug("Wait on 0x{:X} has timed out after {}ns", conditional, timeout);
        state.ctx->gpr.w0 = result;
    }

    void SignalProcessWideKey(const DeviceState &state) {
        auto conditional{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        i32 count{static_cast<i32>(state.ctx->gpr.w1)};

        Logger::Debug("Signalling 0x{:X} for {} waiters", conditional, count);
        state.process->ConditionVariableSignal(conditional, count);
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
            Logger::Warn("Connecting to invalid port: '{}'", port);
            state.ctx->gpr.w0 = result::NotFound;
            return;
        }

        Logger::Debug("Connecting to port '{}' at 0x{:X}", port, handle);

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

        Logger::Debug("0x{:X} -> #{}", handle, tid);

        state.ctx->gpr.x1 = tid;
        state.ctx->gpr.w0 = Result{};
    }

    void Break(const DeviceState &state) {
        auto reason{state.ctx->gpr.x0};
        if (reason & (1ULL << 31)) {
            Logger::Debug("Debugger is being engaged ({})", reason);
        } else {
            Logger::Error("Exit Stack Trace ({}){}", reason, state.loader->GetStackTrace());
            if (state.thread->id)
                state.process->Kill(false);
            std::longjmp(state.thread->originalCtx, true);
        }
    }

    void OutputDebugString(const DeviceState &state) {
        auto string{span(reinterpret_cast<char *>(state.ctx->gpr.x0), state.ctx->gpr.x1).as_string()};

        if (string.back() == '\n')
            string.remove_suffix(1);

        Logger::Info("{}", string);
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
            AslrRegionBaseAddr = 12,
            AslrRegionSize = 13,
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
                out = reinterpret_cast<u64>(state.process->memory.alias.data());
                break;

            case InfoState::AliasRegionSize:
                out = state.process->memory.alias.size();
                break;

            case InfoState::HeapRegionBaseAddr:
                out = reinterpret_cast<u64>(state.process->memory.heap.data());
                break;

            case InfoState::HeapRegionSize:
                out = state.process->memory.heap.size();
                break;

            case InfoState::TotalMemoryAvailable:
                out = std::min(totalPhysicalMemory, state.process->memory.heap.size());
                break;

            case InfoState::TotalMemoryUsage:
                out = state.process->memory.GetUserMemoryUsage() + state.process->memory.GetSystemResourceUsage();
                break;

            case InfoState::IdleTickCount:
                out = 0; // Stubbed
                break;

            case InfoState::RandomEntropy:
                out = util::GetTimeTicks();
                break;

            case InfoState::AslrRegionBaseAddr:
                out = reinterpret_cast<u64>(state.process->memory.base.data());
                break;

            case InfoState::AslrRegionSize:
                out = state.process->memory.base.size();
                break;

            case InfoState::StackRegionBaseAddr:
                out = reinterpret_cast<u64>(state.process->memory.stack.data());
                break;

            case InfoState::StackRegionSize:
                out = state.process->memory.stack.size();
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
                out = std::min(totalPhysicalMemory, state.process->memory.heap.size()) - state.process->npdm.meta.systemResourceSize;
                break;

            case InfoState::TotalMemoryUsageWithoutSystemResource:
                out = state.process->memory.GetUserMemoryUsage();
                break;

            case InfoState::UserExceptionContextAddr:
                out = reinterpret_cast<u64>(state.process->tlsExceptionContext);
                break;

            default:
                Logger::Warn("Unimplemented case ID0: {}, ID1: {}", static_cast<u32>(info), id1);
                state.ctx->gpr.w0 = result::InvalidEnumValue;
                return;
        }

        Logger::Debug("ID0: {}, ID1: {}, Out: 0x{:X}", static_cast<u32>(info), id1, out);

        state.ctx->gpr.x1 = out;
        state.ctx->gpr.w0 = Result{};
    }

    void MapPhysicalMemory(const DeviceState &state) {
        u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        size_t size{state.ctx->gpr.x1};

        if (!util::IsPageAligned(address)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("'address' not page aligned: 0x{:X}", address);
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            Logger::Warn("Invalid address and size combination: 'address': 0x{:X}, 'size': 0x{:X}", address, size);
            return;
        }

        if (!state.process->memory.alias.contains(span<u8>{address, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            Logger::Warn("Tried to map physical memory outside of alias region: 0x{:X} - 0x{:X} (0x{:X} bytes)", address, address + size, size);
            return;
        }

        state.process->memory.MapHeapMemory(span<u8>{address, size});

        Logger::Debug("Mapped physical memory at 0x{:X} - 0x{:X} (0x{:X} bytes)", address, address + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void UnmapPhysicalMemory(const DeviceState &state) {
        u8 *address{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        size_t size{state.ctx->gpr.x1};

        if (!util::IsPageAligned(address)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidAddress;
            Logger::Warn("'address' not page aligned: 0x{:X}", address);
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidSize;
            Logger::Warn("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (!state.process->memory.alias.contains(span<u8>{address, size})) [[unlikely]] {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            Logger::Warn("Tried to unmap physical memory outside of alias region: 0x{:X} - 0x{:X} (0x{:X} bytes)", address, address + size, size);
            return;
        }

        state.process->memory.UnmapMemory(span<u8>{address, size});

        Logger::Debug("Unmapped physical memory at 0x{:X} - 0x{:X} (0x{:X} bytes)", address, address + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void SetThreadActivity(const DeviceState &state) {
        u32 activityValue{static_cast<u32>(state.ctx->gpr.w1)};
        enum class ThreadActivity : u32 {
            Runnable = 0,
            Paused = 1,
        } activity{static_cast<ThreadActivity>(activityValue)};

        switch (activity) {
            case ThreadActivity::Runnable:
            case ThreadActivity::Paused:
                break;

            default:
                Logger::Warn("Invalid thread activity: {}", static_cast<u32>(activity));
                state.ctx->gpr.w0 = result::InvalidEnumValue;
                return;
        }

        KHandle threadHandle{state.ctx->gpr.w0};
        try {
            auto thread{state.process->GetHandle<type::KThread>(threadHandle)};
            if (thread == state.thread) {
                Logger::Warn("Thread setting own activity: {} (Thread: 0x{:X})", static_cast<u32>(activity), threadHandle);
                state.ctx->gpr.w0 = result::Busy;
                return;
            }

            std::scoped_lock guard{thread->coreMigrationMutex};
            if (activity == ThreadActivity::Runnable) {
                if (thread->running && thread->isPaused) {
                    Logger::Debug("Resuming Thread #{}", thread->id);
                    state.scheduler->ResumeThread(thread);
                } else {
                    Logger::Warn("Attempting to resume thread which is already runnable (Thread: 0x{:X})", threadHandle);
                    state.ctx->gpr.w0 = result::InvalidState;
                    return;
                }
            } else if (activity == ThreadActivity::Paused) {
                if (thread->running && !thread->isPaused) {
                    Logger::Debug("Pausing Thread #{}", thread->id);
                    state.scheduler->PauseThread(thread);
                } else {
                    Logger::Warn("Attempting to pause thread which is already paused (Thread: 0x{:X})", threadHandle);
                    state.ctx->gpr.w0 = result::InvalidState;
                    return;
                }
            }

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", static_cast<u32>(threadHandle));
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void GetThreadContext3(const DeviceState &state) {
        KHandle threadHandle{state.ctx->gpr.w1};
        try {
            auto thread{state.process->GetHandle<type::KThread>(threadHandle)};
            if (thread == state.thread) {
                Logger::Warn("Thread attempting to retrieve own context");
                state.ctx->gpr.w0 = result::Busy;
                return;
            }

            std::scoped_lock guard{thread->coreMigrationMutex};
            if (!thread->isPaused) {
                Logger::Warn("Attemping to get context of running thread #{}", thread->id);
                state.ctx->gpr.w0 = result::InvalidState;
                return;
            }

            struct ThreadContext {
                std::array<u64, 29> gpr;
                u64 fp;
                u64 lr;
                u64 sp;
                u64 pc;
                u32 pstate;
                u32 _pad_;
                std::array<u128, 32> vreg;
                u32 fpcr;
                u32 fpsr;
                u64 tpidr;
            };
            static_assert(sizeof(ThreadContext) == 0x320);

            auto &context{*reinterpret_cast<ThreadContext *>(state.ctx->gpr.x0)};
            context = {}; // Zero-initialize the contents of the context as not all fields are set

            auto &targetContext{thread->ctx};
            for (size_t i{}; i < targetContext.gpr.regs.size(); i++)
                context.gpr[i] = targetContext.gpr.regs[i];

            for (size_t i{}; i < targetContext.fpr.regs.size(); i++)
                context.vreg[i] = targetContext.fpr.regs[i];

            context.fpcr = targetContext.fpr.fpcr;
            context.fpsr = targetContext.fpr.fpsr;

            context.tpidr = reinterpret_cast<u64>(targetContext.tpidrEl0);

            // Note: We don't write the whole context as we only store the parts required according to the ARMv8 ABI for syscall handling
            Logger::Debug("Written partial context for thread #{}", thread->id);

            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            Logger::Warn("'handle' invalid: 0x{:X}", threadHandle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void WaitForAddress(const DeviceState &state) {
        auto address{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        if (!util::IsWordAligned(address)) [[unlikely]] {
            Logger::Warn("'address' not word aligned: 0x{:X}", address);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        using ArbitrationType = type::KProcess::ArbitrationType;
        auto arbitrationType{static_cast<ArbitrationType>(static_cast<u32>(state.ctx->gpr.w1))};
        u32 value{state.ctx->gpr.w2};
        i64 timeout{static_cast<i64>(state.ctx->gpr.x3)};

        Result result;
        switch (arbitrationType) {
            case ArbitrationType::WaitIfLessThan:
                Logger::Debug("Waiting on 0x{:X} if less than {} for {}ns", address, value, timeout);
                result = state.process->WaitForAddress(address, value, timeout, ArbitrationType::WaitIfLessThan);
                break;

            case ArbitrationType::DecrementAndWaitIfLessThan:
                Logger::Debug("Waiting on and decrementing 0x{:X} if less than {} for {}ns", address, value, timeout);
                result = state.process->WaitForAddress(address, value, timeout, ArbitrationType::DecrementAndWaitIfLessThan);
                break;

            case ArbitrationType::WaitIfEqual:
                Logger::Debug("Waiting on 0x{:X} if equal to {} for {}ns", address, value, timeout);
                result = state.process->WaitForAddress(address, value, timeout, ArbitrationType::WaitIfEqual);
                break;

            default:
                [[unlikely]]
                    Logger::Error("'arbitrationType' invalid: {}", arbitrationType);
                state.ctx->gpr.w0 = result::InvalidEnumValue;
                return;
        }

        if (result == Result{})
            [[likely]]
                Logger::Debug("Waited on 0x{:X} successfully", address);
        else if (result == result::TimedOut)
            Logger::Debug("Wait on 0x{:X} has timed out after {}ns", address, timeout);
        else if (result == result::InvalidState)
            Logger::Debug("The value at 0x{:X} did not satisfy the arbitration condition", address);

        state.ctx->gpr.w0 = result;
    }

    void SignalToAddress(const DeviceState &state) {
        auto address{reinterpret_cast<u32 *>(state.ctx->gpr.x0)};
        if (!util::IsWordAligned(address)) [[unlikely]] {
            Logger::Warn("'address' not word aligned: 0x{:X}", address);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        using SignalType = type::KProcess::SignalType;
        auto signalType{static_cast<SignalType>(static_cast<u32>(state.ctx->gpr.w1))};
        u32 value{state.ctx->gpr.w2};
        i32 count{static_cast<i32>(state.ctx->gpr.w3)};

        Result result;
        switch (signalType) {
            case SignalType::Signal:
                Logger::Debug("Signalling 0x{:X} for {} waiters", address, count);
                result = state.process->SignalToAddress(address, value, count, SignalType::Signal);
                break;

            case SignalType::SignalAndIncrementIfEqual:
                Logger::Debug("Signalling 0x{:X} and incrementing if equal to {} for {} waiters", address, value, count);
                result = state.process->SignalToAddress(address, value, count, SignalType::SignalAndIncrementIfEqual);
                break;

            case SignalType::SignalAndModifyBasedOnWaitingThreadCountIfEqual:
                Logger::Debug("Signalling 0x{:X} and setting to waiting thread count if equal to {} for {} waiters", address, value, count);
                result = state.process->SignalToAddress(address, value, count, SignalType::SignalAndModifyBasedOnWaitingThreadCountIfEqual);
                break;

            default:
                [[unlikely]]
                    Logger::Error("'signalType' invalid: {}", signalType);
                state.ctx->gpr.w0 = result::InvalidEnumValue;
                return;
        }

        if (result == Result{})
            [[likely]]
                Logger::Debug("Signalled 0x{:X} for {} successfully", address, count);
        else if (result == result::InvalidState)
            Logger::Debug("The value at 0x{:X} did not satisfy the mutation condition", address);

        state.ctx->gpr.w0 = result;
    }
}
