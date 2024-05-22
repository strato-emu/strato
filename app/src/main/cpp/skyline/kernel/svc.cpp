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
    void SetHeapSize(const DeviceState &state, SvcContext &ctx) {
        u32 size{ctx.w1};

        if (!util::IsAligned(size, 0x200000)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            ctx.x1 = 0;

            LOGW("'size' not divisible by 2MB: 0x{:X}", size);
            return;
        } else if (state.process->memory.heap.size() < size) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            ctx.x1 = 0;

            LOGW("'size' exceeded size of heap region: 0x{:X}", size);
            return;
        }

        size_t heapCurrSize{state.process->memory.processHeapSize};
        u8 *heapBaseAddr{state.process->memory.heap.guest.data()};

        if (heapCurrSize < size)
            state.process->memory.MapHeapMemory(span<u8>{heapBaseAddr + heapCurrSize, size - heapCurrSize});
        else if (size < heapCurrSize)
            state.process->memory.UnmapMemory(span<u8>{heapBaseAddr + size, heapCurrSize - size});

        state.process->memory.processHeapSize = size;

        ctx.w0 = Result{};
        ctx.x1 = reinterpret_cast<u64>(heapBaseAddr);

        LOGD("Heap size changed to 0x{:X} bytes ({} - {})", size, fmt::ptr(heapBaseAddr), fmt::ptr(heapBaseAddr + size));
    }

    void SetMemoryPermission(const DeviceState &state, SvcContext &ctx) {
        u8 *address{reinterpret_cast<u8 *>(ctx.x0)};
        if (!util::IsPageAligned(address)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("'address' not page aligned: {}", fmt::ptr(address));
            return;
        }

        u64 size{ctx.x1};
        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
            ctx.w0 = result::InvalidCurrentMemory;
            LOGW("Invalid address and size combination: 'address': {}, 'size': 0x{:X} ", fmt::ptr(address), size);
            return;
        }

        memory::Permission newPermission(static_cast<u8>(ctx.w2));
        if ((!newPermission.r && newPermission.w) || newPermission.x) [[unlikely]] {
            ctx.w0 = result::InvalidNewMemoryPermission;
            LOGW("'permission' invalid: {}", newPermission);
            return;
        }

        auto chunk{state.process->memory.GetChunk(address).value()};
        if (!chunk.second.state.permissionChangeAllowed) [[unlikely]] {
            ctx.w0 = result::InvalidState;
            LOGW("Permission change not allowed for chunk at: {}, state: 0x{:X}", chunk.first, chunk.second.state.value);
            return;
        }

        state.process->memory.SetRegionPermission(span<u8>(address, size), newPermission);

        LOGD("Set permission to {}{}{} at {} - {} (0x{:X} bytes)", newPermission.r ? 'R' : '-', newPermission.w ? 'W' : '-', newPermission.x ? 'X' : '-', fmt::ptr(address), fmt::ptr(address + size), size);
        ctx.w0 = Result{};
    }

    void SetMemoryAttribute(const DeviceState &state, SvcContext &ctx) {
        u8 *address{reinterpret_cast<u8 *>(ctx.x0)};
        if (!util::IsPageAligned(address)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("'address' not page aligned: {}", fmt::ptr(address));
            return;
        }

        u64 size{ctx.x1};
        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
            ctx.w0 = result::InvalidCurrentMemory;
            LOGW("Invalid address and size combination: 'address': {}, 'size': 0x{:X} ", fmt::ptr(address), size);
            return;
        }

        memory::MemoryAttribute mask{static_cast<u8>(ctx.w2)};
        memory::MemoryAttribute value{static_cast<u8>(ctx.w3)};

        auto maskedValue{mask.value | value.value};
        if (maskedValue != mask.value || !mask.isUncached || mask.isDeviceShared || mask.isBorrowed || mask.isIpcLocked) [[unlikely]] {
            ctx.w0 = result::InvalidCombination;
            LOGW("'mask' invalid: 0x{:X}, 0x{:X}", mask.value, value.value);
            return;
        }

        auto chunk{state.process->memory.GetChunk(address).value()};

        // We only check the first found chunk for whatever reason.
        if (!chunk.second.state.attributeChangeAllowed) [[unlikely]] {
            ctx.w0 = result::InvalidState;
            LOGW("Attribute change not allowed for chunk: {}", fmt::ptr(chunk.first));
            return;
        }

        state.process->memory.SetRegionCpuCaching(span<u8>{address, size}, value.isUncached);

        LOGD("Set CPU caching to {} at {} - {} (0x{:X} bytes)", static_cast<bool>(value.isUncached), fmt::ptr(address), fmt::ptr(address + size), size);
        ctx.w0 = Result{};
    }

    void MapMemory(const DeviceState &state, SvcContext &ctx) {
        u8 *destination{reinterpret_cast<u8 *>(ctx.x0)};
        u8 *source{reinterpret_cast<u8 *>(ctx.x1)};
        size_t size{ctx.x2};

        if (!util::IsPageAligned(destination) || !util::IsPageAligned(source)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("Addresses not page aligned: 'source': {}, 'destination': {}, 'size': 0x{:X} bytes", fmt::ptr(source), fmt::ptr(destination), size);
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (destination >= (destination + size) || !state.process->memory.AddressSpaceContains(span<u8>{destination, size})) [[unlikely]] {
            ctx.w0 = result::InvalidCurrentMemory;
            LOGW("Invalid address and size combination: 'destination': {}, 'size': 0x{:X} bytes", fmt::ptr(destination), size);
            return;
        }

        if (source >= (source + size) || !state.process->memory.AddressSpaceContains(span<u8>{source, size})) [[unlikely]] {
            ctx.w0 = result::InvalidCurrentMemory;
            LOGW("Invalid address and size combination: 'source': {}, 'size': 0x{:X} bytes", fmt::ptr(source), size);
            return;
        }

        if (!state.process->memory.stack.guest.contains(span<u8>{destination, size})) [[unlikely]] {
            ctx.w0 = result::InvalidMemoryRegion;
            LOGW("Destination not within stack region: 'source': {}, 'destination': {}, 'size': 0x{:X} bytes", fmt::ptr(source), fmt::ptr(destination), size);
            return;
        }

        auto chunk{state.process->memory.GetChunk(source)};
        if (!chunk->second.state.mapAllowed) [[unlikely]] {
            ctx.w0 = result::InvalidState;
            LOGW("Source doesn't allow usage of svcMapMemory: 'source': {}, 'size': {}, MemoryState: 0x{:X}", fmt::ptr(source), size, chunk->second.state.value);
            return;
        }

        state.process->memory.SvcMapMemory(span<u8>{source, size}, span<u8>{destination, size});

        LOGD("Mapped range {} - {} to {} - {} (Size: 0x{:X} bytes)", fmt::ptr(source), fmt::ptr(source + size), fmt::ptr(destination), fmt::ptr(destination + size), size);
        ctx.w0 = Result{};
    }

    void UnmapMemory(const DeviceState &state, SvcContext &ctx) {
        u8 *destination{reinterpret_cast<u8 *>(ctx.x0)};
        u8 *source{reinterpret_cast<u8 *>(ctx.x1)};
        size_t size{ctx.x2};

        if (!util::IsPageAligned(destination) || !util::IsPageAligned(source)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("Addresses not page aligned: 'source': {}, 'destination': {}, 'size': {} bytes", fmt::ptr(source), fmt::ptr(destination), size);
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (!state.process->memory.stack.guest.contains(span<u8>{destination, size})) [[unlikely]] {
            ctx.w0 = result::InvalidMemoryRegion;
            LOGW("Source not within stack region: 'source': {}, 'destination': {}, 'size': 0x{:X} bytes", fmt::ptr(source), fmt::ptr(destination), size);
            return;
        }

        state.process->memory.SvcUnmapMemory(span<u8>{source, size}, span<u8>{destination, size});
        state.process->memory.UnmapMemory(span<u8>{destination, size});

        LOGD("Unmapped range {} - {} to {} - {} (Size: 0x{:X} bytes)", fmt::ptr(destination), fmt::ptr(destination + size), source, source + size, size);
        ctx.w0 = Result{};
    }

    void QueryMemory(const DeviceState &state, SvcContext &ctx) {
        memory::MemoryInfo memInfo{};

        u8 *address{reinterpret_cast<u8 *>(ctx.x2)};
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

            LOGD("Address: {}, Region Start: 0x{:X}, Size: 0x{:X}, Type: 0x{:X}, Attributes: 0x{:X}, Permissions: {}", fmt::ptr(address), memInfo.address, memInfo.size, memInfo.type, memInfo.attributes, chunk->second.permission);
        } else {
            u64 addressSpaceEnd{reinterpret_cast<u64>(state.process->memory.addressSpace.end().base())};

            memInfo = {
                .address = addressSpaceEnd,
                .size = 0 - addressSpaceEnd,
                .type = static_cast<u32>(memory::MemoryType::Reserved),
            };

            LOGD("Trying to query memory outside of the application's address space: {}", fmt::ptr(address));
        }

        *reinterpret_cast<memory::MemoryInfo *>(ctx.x0) = memInfo;
        // The page info, which is always 0
        ctx.w1 = 0;

        ctx.w0 = Result{};
    }

    void ExitProcess(const DeviceState &state, SvcContext &ctx) {
        LOGD("Exiting process");
        throw nce::NCE::ExitException(true);
    }

    constexpr i32 IdealCoreDontCare{-1};
    constexpr i32 IdealCoreUseProcessValue{-2};
    constexpr i32 IdealCoreNoUpdate{-3};

    void CreateThread(const DeviceState &state, SvcContext &ctx) {
        auto entry{reinterpret_cast<void *>(ctx.x1)};
        auto entryArgument{ctx.x2};
        auto stackTop{reinterpret_cast<u8 *>(ctx.x3)};
        auto priority{static_cast<i8>(ctx.w4)};
        auto idealCore{static_cast<i32>(ctx.w5)};

        idealCore = (idealCore == IdealCoreUseProcessValue) ? static_cast<i32>(state.process->npdm.meta.idealCore) : idealCore;
        if (idealCore < 0 || idealCore >= constant::CoreCount) {
            ctx.w0 = result::InvalidCoreId;
            LOGW("'idealCore' invalid: {}", idealCore);
            return;
        }

        if (!state.process->npdm.threadInfo.priority.Valid(priority)) {
            ctx.w0 = result::InvalidPriority;
            LOGW("'priority' invalid: {}", priority);
            return;
        }

        auto thread{state.process->CreateThread(entry, entryArgument, stackTop, priority, static_cast<u8>(idealCore))};
        if (thread) {
            LOGD("Created thread #{} with handle 0x{:X} (Entry Point: {}, Argument: 0x{:X}, Stack Pointer: {}, Priority: {}, Ideal Core: {})", thread->id, thread->handle, entry, entryArgument, fmt::ptr(stackTop), priority, idealCore);

            ctx.w1 = thread->handle;
            ctx.w0 = Result{};
        } else {
            LOGD("Cannot create thread (Entry Point: {}, Argument: 0x{:X}, Stack Pointer: {}, Priority: {}, Ideal Core: {})", entry, entryArgument, fmt::ptr(stackTop), priority, idealCore);
            ctx.w1 = 0;
            ctx.w0 = result::OutOfResource;
        }
    }

    void StartThread(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w0};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            LOGD("Starting thread #{}: 0x{:X}", thread->id, handle);
            thread->Start();
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void ExitThread(const DeviceState &state, SvcContext &ctx) {
        LOGD("Exiting current thread");
        throw nce::NCE::ExitException(false);
    }

    void SleepThread(const DeviceState &state, SvcContext &ctx) {
        constexpr i64 yieldWithoutCoreMigration{0};
        constexpr i64 yieldWithCoreMigration{-1};
        constexpr i64 yieldToAnyThread{-2};

        i64 in{static_cast<i64>(ctx.x0)};
        if (in > 0) {
            LOGD("Sleeping for {}ns", in);
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
                    LOGD("Waking any appropriate parked threads and yielding");
                    TRACE_EVENT("kernel", "YieldWithCoreMigration");
                    state.scheduler->WakeParkedThread();
                    state.scheduler->Rotate();
                    state.scheduler->WaitSchedule();
                    break;
                }

                case yieldWithoutCoreMigration: {
                    LOGD("Cooperative yield");
                    TRACE_EVENT("kernel", "YieldWithoutCoreMigration");
                    state.scheduler->Rotate();
                    state.scheduler->WaitSchedule();
                    break;
                }

                case yieldToAnyThread: {
                    LOGD("Parking current thread");
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

    void GetThreadPriority(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w1};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            i8 priority{thread->priority};
            LOGD("Retrieving thread #{}'s priority: {}", thread->id, priority);

            ctx.w1 = static_cast<u32>(priority);
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void SetThreadPriority(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w0};
        i8 priority{static_cast<i8>(ctx.w1)};
        if (!state.process->npdm.threadInfo.priority.Valid(priority)) {
            LOGW("'priority' invalid: 0x{:X}", priority);
            ctx.w0 = result::InvalidPriority;
            return;
        }
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            LOGD("Setting thread #{}'s priority to {}", thread->id, priority);
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
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void GetThreadCoreMask(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w2};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            auto idealCore{thread->idealCore};
            auto affinityMask{thread->affinityMask};
            LOGD("Getting thread #{}'s Ideal Core ({}) + Affinity Mask ({})", thread->id, idealCore, affinityMask);

            ctx.x2 = affinityMask.to_ullong();
            ctx.w1 = static_cast<u32>(idealCore);
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void SetThreadCoreMask(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w0};
        i32 idealCore{static_cast<i32>(ctx.w1)};
        CoreMask affinityMask{ctx.x2};
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
                LOGW("'affinityMask' invalid: {} (Process Mask: {})", affinityMask, processMask);
                ctx.w0 = result::InvalidCoreId;
                return;
            }

            if (affinityMask.none() || !affinityMask.test(static_cast<size_t>(idealCore))) {
                LOGW("'affinityMask' invalid: {} (Ideal Core: {})", affinityMask, idealCore);
                ctx.w0 = result::InvalidCombination;
                return;
            }

            LOGD("Setting thread #{}'s Ideal Core ({}) + Affinity Mask ({})", thread->id, idealCore, affinityMask);

            std::scoped_lock guard{thread->coreMigrationMutex};
            thread->idealCore = static_cast<u8>(idealCore);
            thread->affinityMask = affinityMask;

            if (!affinityMask.test(static_cast<size_t>(thread->coreId)) && thread->coreId != constant::ParkedCoreId) {
                LOGD("Migrating thread #{} to Ideal Core C{} -> C{}", thread->id, thread->coreId, idealCore);

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

            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void GetCurrentProcessorNumber(const DeviceState &state, SvcContext &ctx) {
        std::scoped_lock guard{state.thread->coreMigrationMutex};
        u8 coreId{state.thread->coreId};
        LOGD("C{}", coreId);
        ctx.w0 = coreId;
    }

    void ClearEvent(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w0};
        TRACE_EVENT_FMT("kernel", "ClearEvent 0x{:X}", handle);
        try {
            std::static_pointer_cast<type::KEvent>(state.process->GetHandle(handle))->ResetSignal();
            LOGD("Clearing 0x{:X}", handle);
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
            return;
        }
    }

    void MapSharedMemory(const DeviceState &state, SvcContext &ctx) {
        try {
            KHandle handle{ctx.w0};
            auto object{state.process->GetHandle<type::KSharedMemory>(handle)};
            u8 *address{reinterpret_cast<u8 *>(ctx.x1)};

            if (!util::IsPageAligned(address)) [[unlikely]] {
                ctx.w0 = result::InvalidAddress;
                LOGW("'address' not page aligned: {}", fmt::ptr(address));
                return;
            }

            size_t size{ctx.x2};
            if (!size || !util::IsPageAligned(size)) [[unlikely]] {
                ctx.w0 = result::InvalidSize;
                LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
                return;
            }

            if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
                ctx.w0 = result::InvalidCurrentMemory;
                LOGW("Invalid address and size combination: 'address': {}, 'size': 0x{:X}", fmt::ptr(address), size);
                return;
            }

            memory::Permission permission(static_cast<u8>(ctx.w3));
            if ((!permission.r && !permission.w && !permission.x) || (permission.w && !permission.r) || permission.x) [[unlikely]] {
                ctx.w0 = result::InvalidNewMemoryPermission;
                LOGW("'permission' invalid: {}", permission);
                return;
            }

            LOGD("Mapping shared memory (0x{:X}) at {} - {} (0x{:X} bytes), with permissions: ({}{}{})", handle, fmt::ptr(address), fmt::ptr(address + size), size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

            object->Map(span<u8>{address, size}, permission);
            state.process->memory.AddRef(object);

            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", static_cast<u32>(ctx.w0));
            ctx.w0 = result::InvalidHandle;
        }
    }

    void UnmapSharedMemory(const DeviceState &state, SvcContext &ctx) {
        try {
            KHandle handle{ctx.w0};
            auto object{state.process->GetHandle<type::KSharedMemory>(handle)};
            u8 *address{reinterpret_cast<u8 *>(ctx.x1)};

            if (!util::IsPageAligned(address)) [[unlikely]] {
                ctx.w0 = result::InvalidAddress;
                LOGW("'address' not page aligned: {}", fmt::ptr(address));
                return;
            }

            size_t size{ctx.x2};
            if (!size || !util::IsPageAligned(size)) [[unlikely]] {
                ctx.w0 = result::InvalidSize;
                LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
                return;
            }

            if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
                ctx.w0 = result::InvalidCurrentMemory;
                LOGW("Invalid address and size combination: 'address': {}, 'size': 0x{:X}", fmt::ptr(address), size);
                return;
            }

            LOGD("Unmapping shared memory (0x{:X}) at {} - {} (0x{:X} bytes)", handle, fmt::ptr(address), fmt::ptr(address + size), size);

            object->Unmap(span<u8>{address, size});
            state.process->memory.RemoveRef(object);

            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", static_cast<u32>(ctx.w0));
            ctx.w0 = result::InvalidHandle;
        }
    }

    void CreateTransferMemory(const DeviceState &state, SvcContext &ctx) {
        u8 *address{reinterpret_cast<u8 *>(ctx.x1)};
        if (!util::IsPageAligned(address)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("'address' not page aligned: {}", fmt::ptr(address));
            return;
        }

        size_t size{ctx.x2};
        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size) || !state.process->memory.AddressSpaceContains(span<u8>{address, size})) [[unlikely]] {
            ctx.w0 = result::InvalidCurrentMemory;
            LOGW("Invalid address and size combination: 'address': {}, 'size': 0x{:X}", fmt::ptr(address), size);
            return;
        }

        memory::Permission permission(static_cast<u8>(ctx.w3));
        if ((permission.w && !permission.r) || permission.x) [[unlikely]] {
            LOGW("'permission' invalid: {}", permission);
            ctx.w0 = result::InvalidNewMemoryPermission;
            return;
        }

        auto tmem{state.process->NewHandle<kernel::type::KTransferMemory>(size)};
        if (!tmem.item->Map(span<u8>{address, size}, permission)) [[unlikely]] {
            ctx.w0 = result::InvalidState;
            return;
        }

        LOGD("Creating transfer memory (0x{:X}) at {} - {} (0x{:X} bytes) ({}{}{})", tmem.handle, fmt::ptr(address), fmt::ptr(address + size), size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

        ctx.w0 = Result{};
        ctx.w1 = tmem.handle;
    }

    void CloseHandle(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{static_cast<KHandle>(ctx.w0)};
        try {
            state.process->CloseHandle(handle);
            LOGD("Closing 0x{:X}", handle);
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void ResetSignal(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w0};
        TRACE_EVENT_FMT("kernel", "ResetSignal 0x{:X}", handle);
        try {
            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KEvent:
                case type::KType::KProcess:
                    ctx.w0 = std::static_pointer_cast<type::KSyncObject>(object)->ResetSignal() ? Result{} : result::InvalidState;
                    break;

                default: {
                    LOGW("'handle' type invalid: 0x{:X} ({})", handle, object->objectType);
                    ctx.w0 = result::InvalidHandle;
                    return;
                }
            }

            LOGD("Resetting 0x{:X}", handle);
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", handle);
            ctx.w0 = result::InvalidHandle;
            return;
        }
    }

    void WaitSynchronization(const DeviceState &state, SvcContext &ctx) {
        constexpr u8 MaxSyncHandles{0x40}; // The total amount of handles that can be passed to WaitSynchronization

        u32 numHandles{ctx.w2};
        if (numHandles > MaxSyncHandles) {
            ctx.w0 = result::OutOfRange;
            return;
        }

        span waitHandles(reinterpret_cast<KHandle *>(ctx.x1), numHandles);
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
                    LOGD("An invalid handle was supplied: 0x{:X}", handle);
                    ctx.w0 = result::InvalidHandle;
                    return;
                }
            }
        }

        i64 timeout{static_cast<i64>(ctx.x3)};
        if (waitHandles.size() == 1) {
            LOGD("Waiting on 0x{:X} for {}ns", waitHandles[0], timeout);
        } else if (AsyncLogger::CheckLogLevel(AsyncLogger::LogLevel::Debug)) {
            std::string handleString;
            for (const auto &handle : waitHandles)
                handleString += fmt::format("* 0x{:X}\n", handle);
            LOGD("Waiting on handles:\n{}Timeout: {}ns", handleString, timeout);
        }

        TRACE_EVENT_FMT("kernel", fmt::runtime(waitHandles.size() == 1 ? "WaitSynchronization 0x{:X}" : "WaitSynchronizationMultiple 0x{:X}"), waitHandles[0]);

        std::unique_lock lock(type::KSyncObject::syncObjectMutex);
        if (state.thread->cancelSync) {
            state.thread->cancelSync = false;
            ctx.w0 = result::Cancelled;
            return;
        }

        u32 index{};
        for (const auto &object : objectTable) {
            if (object->signalled) {
                LOGD("Signalled 0x{:X}", waitHandles[index]);
                ctx.w0 = Result{};
                ctx.w1 = index;
                return;
            }
            index++;
        }

        if (timeout == 0) {
            LOGD("No handle is currently signalled");
            ctx.w0 = result::TimedOut;
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
            LOGD("Signalled 0x{:X}", waitHandles[wakeIndex]);
            ctx.w0 = Result{};
            ctx.w1 = wakeIndex;
        } else if (state.thread->cancelSync) {
            state.thread->cancelSync = false;
            LOGD("Wait has been cancelled");
            ctx.w0 = result::Cancelled;
        } else {
            LOGD("Wait has timed out");
            ctx.w0 = result::TimedOut;
            lock.unlock();
            state.scheduler->InsertThread(state.thread);
            state.scheduler->WaitSchedule();
        }
    }

    void CancelSynchronization(const DeviceState &state, SvcContext &ctx) {
        try {
            std::unique_lock lock(type::KSyncObject::syncObjectMutex);
            auto thread{state.process->GetHandle<type::KThread>(ctx.w0)};
            LOGD("Cancelling Synchronization {}", thread->id);
            thread->cancelSync = true;
            if (thread->isCancellable) {
                thread->isCancellable = false;
                state.scheduler->InsertThread(thread);
            }
            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", static_cast<u32>(ctx.w0));
            ctx.w0 = result::InvalidHandle;
        }
    }

    void ArbitrateLock(const DeviceState &state, SvcContext &ctx) {
        auto mutex{reinterpret_cast<u32 *>(ctx.x1)};
        if (!util::IsWordAligned(mutex)) {
            LOGW("'mutex' not word aligned: {}", fmt::ptr(mutex));
            ctx.w0 = result::InvalidAddress;
            return;
        }

        LOGD("Locking {}", fmt::ptr(mutex));

        KHandle ownerHandle{ctx.w0};
        KHandle requesterHandle{ctx.w2};
        auto result{state.process->MutexLock(state.thread, mutex, ownerHandle, requesterHandle)};
        if (result == Result{})
            LOGD("Locked {}", fmt::ptr(mutex));
        else if (result == result::InvalidCurrentMemory)
            result = Result{}; // If the mutex value isn't expected then it's still successful
        else if (result == result::InvalidHandle)
            LOGW("'ownerHandle' invalid: 0x{:X} ({})", ownerHandle, fmt::ptr(mutex));

        ctx.w0 = result;
    }

    void ArbitrateUnlock(const DeviceState &state, SvcContext &ctx) {
        auto mutex{reinterpret_cast<u32 *>(ctx.x0)};
        if (!util::IsWordAligned(mutex)) {
            LOGW("'mutex' not word aligned: {}", fmt::ptr(mutex));
            ctx.w0 = result::InvalidAddress;
            return;
        }

        LOGD("Unlocking {}", fmt::ptr(mutex));
        state.process->MutexUnlock(mutex);
        LOGD("Unlocked {}", fmt::ptr(mutex));

        ctx.w0 = Result{};
    }

    void WaitProcessWideKeyAtomic(const DeviceState &state, SvcContext &ctx) {
        auto mutex{reinterpret_cast<u32 *>(ctx.x0)};
        if (!util::IsWordAligned(mutex)) {
            LOGW("'mutex' not word aligned: {}", fmt::ptr(mutex));
            ctx.w0 = result::InvalidAddress;
            return;
        }

        auto conditional{reinterpret_cast<u32 *>(ctx.x1)};
        KHandle requesterHandle{ctx.w2};

        i64 timeout{static_cast<i64>(ctx.x3)};
        LOGD("Waiting on {} with {} for {}ns", fmt::ptr(conditional), fmt::ptr(mutex), timeout);

        auto result{state.process->ConditionVariableWait(conditional, mutex, requesterHandle, timeout)};
        if (result == Result{})
            LOGD("Waited for {} and reacquired {}", fmt::ptr(conditional), fmt::ptr(mutex));
        else if (result == result::TimedOut)
            LOGD("Wait on {} has timed out after {}ns", fmt::ptr(conditional), timeout);
        ctx.w0 = result;
    }

    void SignalProcessWideKey(const DeviceState &state, SvcContext &ctx) {
        auto conditional{reinterpret_cast<u32 *>(ctx.x0)};
        i32 count{static_cast<i32>(ctx.w1)};

        LOGD("Signalling {} for {} waiters", fmt::ptr(conditional), count);
        state.process->ConditionVariableSignal(conditional, count);
        ctx.w0 = Result{};
    }

    void GetSystemTick(const DeviceState &state, SvcContext &ctx) {
        u64 tick;
        asm("STR X1, [SP, #-16]!\n\t"
            "MRS %0, CNTVCT_EL0\n\t"
            "MOV X1, #0xF800\n\t"
            "MOVK X1, #0x124, lsl #16\n\t"
            "MUL %0, %0, X1\n\t"
            "MRS X1, CNTFRQ_EL0\n\t"
            "UDIV %0, %0, X1\n\t"
            "LDR X1, [SP], #16" : "=r"(tick));
        ctx.x0 = tick;
    }

    void ConnectToNamedPort(const DeviceState &state, SvcContext &ctx) {
        constexpr u8 portSize = 0x8; //!< The size of a port name string
        std::string_view port(span(reinterpret_cast<char *>(ctx.x1), portSize).as_string(true));

        KHandle handle{};
        if (port.compare("sm:") >= 0) {
            handle = state.process->NewHandle<type::KSession>(std::static_pointer_cast<service::BaseService>(state.os->serviceManager.smUserInterface)).handle;
        } else {
            LOGW("Connecting to invalid port: '{}'", port);
            ctx.w0 = result::NotFound;
            return;
        }

        LOGD("Connecting to port '{}' at 0x{:X}", port, handle);

        ctx.w1 = handle;
        ctx.w0 = Result{};
    }

    void SendSyncRequest(const DeviceState &state, SvcContext &ctx) {
        SchedulerScopedLock schedulerLock(state);
        state.os->serviceManager.SyncRequestHandler(static_cast<KHandle>(ctx.x0));
        ctx.w0 = Result{};
    }

    void GetThreadId(const DeviceState &state, SvcContext &ctx) {
        KHandle handle{ctx.w1};
        size_t tid{state.process->GetHandle<type::KThread>(handle)->id};

        LOGD("0x{:X} -> #{}", handle, tid);

        ctx.x1 = tid;
        ctx.w0 = Result{};
    }

    void Break(const DeviceState &state, SvcContext &ctx) {
        auto reason{ctx.x0};
        if (reason & (1ULL << 31)) {
            LOGD("Debugger is being engaged ({})", reason);
        } else {
            LOGE("Exit Stack Trace ({}){}", reason, state.loader->GetStackTrace());
            if (state.thread->id)
                state.process->Kill(false);
            std::longjmp(state.thread->originalCtx, true);
        }
    }

    void OutputDebugString(const DeviceState &state, SvcContext &ctx) {
        auto string{span(reinterpret_cast<char *>(ctx.x0), ctx.x1).as_string()};

        if (string.back() == '\n')
            string.remove_suffix(1);

        LOGI("{}", string);
        ctx.w0 = Result{};
    }

    void GetInfo(const DeviceState &state, SvcContext &ctx) {
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

        InfoState info{static_cast<u32>(ctx.w1)};
        KHandle handle{ctx.w2};
        u64 id1{ctx.x3};

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
                out = reinterpret_cast<u64>(state.process->memory.alias.guest.data());
                break;

            case InfoState::AliasRegionSize:
                out = state.process->memory.alias.size();
                break;

            case InfoState::HeapRegionBaseAddr:
                out = reinterpret_cast<u64>(state.process->memory.heap.guest.data());
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
                out = reinterpret_cast<u64>(state.process->memory.stack.guest.data());
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
                LOGW("Unimplemented case ID0: {}, ID1: {}", static_cast<u32>(info), id1);
                ctx.w0 = result::InvalidEnumValue;
                return;
        }

        LOGD("ID0: {}, ID1: {}, Out: 0x{:X}", static_cast<u32>(info), id1, out);

        ctx.x1 = out;
        ctx.w0 = Result{};
    }

    void MapPhysicalMemory(const DeviceState &state, SvcContext &ctx) {
        u8 *address{reinterpret_cast<u8 *>(ctx.x0)};
        size_t size{ctx.x1};

        if (!util::IsPageAligned(address)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("'address' not page aligned: {}", fmt::ptr(address));
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (address >= (address + size)) [[unlikely]] {
            ctx.w0 = result::InvalidMemoryRegion;
            LOGW("Invalid address and size combination: 'address': {}, 'size': 0x{:X}", fmt::ptr(address), size);
            return;
        }

        if (!state.process->memory.alias.guest.contains(span<u8>{address, size})) [[unlikely]] {
            ctx.w0 = result::InvalidMemoryRegion;
            LOGW("Tried to map physical memory outside of alias region: {} - {} (0x{:X} bytes)", fmt::ptr(address), fmt::ptr(address + size), size);
            return;
        }

        state.process->memory.MapHeapMemory(span<u8>{address, size});

        LOGD("Mapped physical memory at {} - {} (0x{:X} bytes)", fmt::ptr(address), fmt::ptr(address + size), size);
        ctx.w0 = Result{};
    }

    void UnmapPhysicalMemory(const DeviceState &state, SvcContext &ctx) {
        u8 *address{reinterpret_cast<u8 *>(ctx.x0)};
        size_t size{ctx.x1};

        if (!util::IsPageAligned(address)) [[unlikely]] {
            ctx.w0 = result::InvalidAddress;
            LOGW("'address' not page aligned: {}", fmt::ptr(address));
            return;
        }

        if (!size || !util::IsPageAligned(size)) [[unlikely]] {
            ctx.w0 = result::InvalidSize;
            LOGW("'size' {}: 0x{:X}", size ? "is not page aligned" : "is zero", size);
            return;
        }

        if (!state.process->memory.alias.guest.contains(span<u8>{address, size})) [[unlikely]] {
            ctx.w0 = result::InvalidMemoryRegion;
            LOGW("Tried to unmap physical memory outside of alias region: {} - {} (0x{:X} bytes)", fmt::ptr(address), fmt::ptr(address + size), size);
            return;
        }

        state.process->memory.UnmapMemory(span<u8>{address, size});

        LOGD("Unmapped physical memory at {} - {} (0x{:X} bytes)", fmt::ptr(address), fmt::ptr(address + size), size);
        ctx.w0 = Result{};
    }

    void SetThreadActivity(const DeviceState &state, SvcContext &ctx) {
        u32 activityValue{static_cast<u32>(ctx.w1)};
        enum class ThreadActivity : u32 {
            Runnable = 0,
            Paused = 1,
        } activity{static_cast<ThreadActivity>(activityValue)};

        switch (activity) {
            case ThreadActivity::Runnable:
            case ThreadActivity::Paused:
                break;

            default:
                LOGW("Invalid thread activity: {}", static_cast<u32>(activity));
                ctx.w0 = result::InvalidEnumValue;
                return;
        }

        KHandle threadHandle{ctx.w0};
        try {
            auto thread{state.process->GetHandle<type::KThread>(threadHandle)};
            if (thread == state.thread) {
                LOGW("Thread setting own activity: {} (Thread: 0x{:X})", static_cast<u32>(activity), threadHandle);
                ctx.w0 = result::Busy;
                return;
            }

            std::scoped_lock guard{thread->coreMigrationMutex};
            if (activity == ThreadActivity::Runnable) {
                if (thread->running && thread->isPaused) {
                    LOGD("Resuming Thread #{}", thread->id);
                    state.scheduler->ResumeThread(thread);
                } else {
                    LOGW("Attempting to resume thread which is already runnable (Thread: 0x{:X})", threadHandle);
                    ctx.w0 = result::InvalidState;
                    return;
                }
            } else if (activity == ThreadActivity::Paused) {
                if (thread->running && !thread->isPaused) {
                    LOGD("Pausing Thread #{}", thread->id);
                    state.scheduler->PauseThread(thread);
                } else {
                    LOGW("Attempting to pause thread which is already paused (Thread: 0x{:X})", threadHandle);
                    ctx.w0 = result::InvalidState;
                    return;
                }
            }

            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", static_cast<u32>(threadHandle));
            ctx.w0 = result::InvalidHandle;
        }
    }

    void GetThreadContext3(const DeviceState &state, SvcContext &ctx) {
        KHandle threadHandle{ctx.w1};
        try {
            auto thread{state.process->GetHandle<type::KThread>(threadHandle)};
            if (thread == state.thread) {
                LOGW("Thread attempting to retrieve own context");
                ctx.w0 = result::Busy;
                return;
            }

            std::scoped_lock guard{thread->coreMigrationMutex};
            if (!thread->isPaused) {
                LOGW("Attemping to get context of running thread #{}", thread->id);
                ctx.w0 = result::InvalidState;
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

            auto &context{*reinterpret_cast<ThreadContext *>(ctx.x0)};
            context = {}; // Zero-initialize the contents of the context as not all fields are set

            if (state.process->is64bit()) {
                auto &targetContext{dynamic_cast<type::KNceThread *>(thread.get())->ctx};
                for (size_t i{}; i < targetContext.gpr.regs.size(); i++)
                    context.gpr[i] = targetContext.gpr.regs[i];

                for (size_t i{}; i < targetContext.fpr.regs.size(); i++)
                    context.vreg[i] = targetContext.fpr.regs[i];

                context.fpcr = targetContext.fpr.fpcr;
                context.fpsr = targetContext.fpr.fpsr;

                context.tpidr = reinterpret_cast<u64>(targetContext.tpidrEl0);
            } else { // 32 bit
                constexpr u32 El0Aarch32PsrMask = 0xFE0FFE20;
                // https://developer.arm.com/documentation/ddi0601/2023-12/AArch32-Registers/FPSCR--Floating-Point-Status-and-Control-Register
                constexpr u32 FpsrMask = 0xF800009F; // [31:27], [7], [4:0]
                constexpr u32 FpcrMask = 0x07FF9F00; // [26:15], [12:8]

                auto &targetContext{dynamic_cast<type::KJit32Thread *>(thread.get())->ctx};

                context.pc = targetContext.pc;
                context.pstate = targetContext.cpsr & El0Aarch32PsrMask;

                for (size_t i{}; i < targetContext.gpr.size() - 1; i++)
                    context.gpr[i] = targetContext.gpr[i];

                // TODO: Check if this is correct
                for (size_t i{}; i < targetContext.fpr.size(); i++) {
                    context.vreg[i] = targetContext.fpr_d[i];
                }

                context.fpsr = targetContext.fpscr & FpsrMask;
                context.fpcr = targetContext.fpscr & FpcrMask;
                context.tpidr = targetContext.tpidr;
            }

            // Note: We don't write the whole context as we only store the parts required according to the ARMv8 ABI for syscall handling
            LOGD("Written partial context for thread #{}", thread->id);

            ctx.w0 = Result{};
        } catch (const std::out_of_range &) {
            LOGW("'handle' invalid: 0x{:X}", threadHandle);
            ctx.w0 = result::InvalidHandle;
        }
    }

    void WaitForAddress(const DeviceState &state, SvcContext &ctx) {
        auto address{reinterpret_cast<u32 *>(ctx.x0)};
        if (!util::IsWordAligned(address)) [[unlikely]] {
            LOGW("'address' not word aligned: {}", fmt::ptr(address));
            ctx.w0 = result::InvalidAddress;
            return;
        }

        using ArbitrationType = type::KProcess::ArbitrationType;
        auto arbitrationType{static_cast<ArbitrationType>(static_cast<u32>(ctx.w1))};
        u32 value{ctx.w2};
        i64 timeout{static_cast<i64>(ctx.x3)};

        Result result;
        switch (arbitrationType) {
            case ArbitrationType::WaitIfLessThan:
                LOGD("Waiting on {} if less than {} for {}ns", fmt::ptr(address), value, timeout);
                result = state.process->WaitForAddress(address, value, timeout, ArbitrationType::WaitIfLessThan);
                break;

            case ArbitrationType::DecrementAndWaitIfLessThan:
                LOGD("Waiting on and decrementing {} if less than {} for {}ns", fmt::ptr(address), value, timeout);
                result = state.process->WaitForAddress(address, value, timeout, ArbitrationType::DecrementAndWaitIfLessThan);
                break;

            case ArbitrationType::WaitIfEqual:
                LOGD("Waiting on {} if equal to {} for {}ns", fmt::ptr(address), value, timeout);
                result = state.process->WaitForAddress(address, value, timeout, ArbitrationType::WaitIfEqual);
                break;

            default:
                [[unlikely]]
                LOGE("'arbitrationType' invalid: {}", arbitrationType);
                ctx.w0 = result::InvalidEnumValue;
                return;
        }

        if (result == Result{})
            [[likely]]
            LOGD("Waited on {} successfully", fmt::ptr(address));
        else if (result == result::TimedOut)
            LOGD("Wait on {} has timed out after {}ns", fmt::ptr(address), timeout);
        else if (result == result::InvalidState)
            LOGD("The value at {} did not satisfy the arbitration condition", fmt::ptr(address));

        ctx.w0 = result;
    }

    void SignalToAddress(const DeviceState &state, SvcContext &ctx) {
        auto address{reinterpret_cast<u32 *>(ctx.x0)};
        if (!util::IsWordAligned(address)) [[unlikely]] {
            LOGW("'address' not word aligned: {}", fmt::ptr(address));
            ctx.w0 = result::InvalidAddress;
            return;
        }

        using SignalType = type::KProcess::SignalType;
        auto signalType{static_cast<SignalType>(static_cast<u32>(ctx.w1))};
        u32 value{ctx.w2};
        i32 count{static_cast<i32>(ctx.w3)};

        Result result;
        switch (signalType) {
            case SignalType::Signal:
                LOGD("Signalling {} for {} waiters", fmt::ptr(address), count);
                result = state.process->SignalToAddress(address, value, count, SignalType::Signal);
                break;

            case SignalType::SignalAndIncrementIfEqual:
                LOGD("Signalling {} and incrementing if equal to {} for {} waiters", fmt::ptr(address), value, count);
                result = state.process->SignalToAddress(address, value, count, SignalType::SignalAndIncrementIfEqual);
                break;

            case SignalType::SignalAndModifyBasedOnWaitingThreadCountIfEqual:
                LOGD("Signalling {} and setting to waiting thread count if equal to {} for {} waiters", fmt::ptr(address), value, count);
                result = state.process->SignalToAddress(address, value, count, SignalType::SignalAndModifyBasedOnWaitingThreadCountIfEqual);
                break;

            default:
                [[unlikely]]
                LOGE("'signalType' invalid: {}", signalType);
                ctx.w0 = result::InvalidEnumValue;
                return;
        }

        if (result == Result{}) [[likely]]
            LOGD("Signalled {} for {} successfully", fmt::ptr(address), count);
        else if (result == result::InvalidState)
            LOGD("The value at {} did not satisfy the mutation condition", fmt::ptr(address));

        ctx.w0 = result;
    }

    #define SVC_NONE SvcDescriptor{} //!< A macro with a placeholder value for the SVC not being implemented or not existing
    #define SVC_STRINGIFY(name) #name
    #define SVC_ENTRY(function) SvcDescriptor{function, SVC_STRINGIFY(Svc ## function)} //!< A macro which automatically stringifies the function name as the name to prevent pointless duplication

    constexpr std::array<SvcDescriptor, 0x80> SvcTable{
        SVC_NONE, // 0x00 (Does not exist)
        SVC_ENTRY(SetHeapSize), // 0x01
        SVC_ENTRY(SetMemoryPermission), // 0x02
        SVC_ENTRY(SetMemoryAttribute), // 0x03
        SVC_ENTRY(MapMemory), // 0x04
        SVC_ENTRY(UnmapMemory), // 0x05
        SVC_ENTRY(QueryMemory), // 0x06
        SVC_ENTRY(ExitProcess), // 0x07
        SVC_ENTRY(CreateThread), // 0x08
        SVC_ENTRY(StartThread), // 0x09
        SVC_ENTRY(ExitThread), // 0x0A
        SVC_ENTRY(SleepThread), // 0x0B
        SVC_ENTRY(GetThreadPriority), // 0x0C
        SVC_ENTRY(SetThreadPriority), // 0x0D
        SVC_ENTRY(GetThreadCoreMask), // 0x0E
        SVC_ENTRY(SetThreadCoreMask), // 0x0F
        SVC_ENTRY(GetCurrentProcessorNumber), // 0x10
        SVC_NONE, // 0x11
        SVC_ENTRY(ClearEvent), // 0x12
        SVC_ENTRY(MapSharedMemory), // 0x13
        SVC_ENTRY(UnmapSharedMemory), // 0x14
        SVC_ENTRY(CreateTransferMemory), // 0x15
        SVC_ENTRY(CloseHandle), // 0x16
        SVC_ENTRY(ResetSignal), // 0x17
        SVC_ENTRY(WaitSynchronization), // 0x18
        SVC_ENTRY(CancelSynchronization), // 0x19
        SVC_ENTRY(ArbitrateLock), // 0x1A
        SVC_ENTRY(ArbitrateUnlock), // 0x1B
        SVC_ENTRY(WaitProcessWideKeyAtomic), // 0x1C
        SVC_ENTRY(SignalProcessWideKey), // 0x1D
        SVC_ENTRY(GetSystemTick), // 0x1E
        SVC_ENTRY(ConnectToNamedPort), // 0x1F
        SVC_NONE, // 0x20
        SVC_ENTRY(SendSyncRequest), // 0x21
        SVC_NONE, // 0x22
        SVC_NONE, // 0x23
        SVC_NONE, // 0x24
        SVC_ENTRY(GetThreadId), // 0x25
        SVC_ENTRY(Break), // 0x26
        SVC_ENTRY(OutputDebugString), // 0x27
        SVC_NONE, // 0x28
        SVC_ENTRY(GetInfo), // 0x29
        SVC_NONE, // 0x2A
        SVC_NONE, // 0x2B
        SVC_ENTRY(MapPhysicalMemory), // 0x2C
        SVC_ENTRY(UnmapPhysicalMemory), // 0x2D
        SVC_NONE, // 0x2E
        SVC_NONE, // 0x2F
        SVC_NONE, // 0x30
        SVC_NONE, // 0x31
        SVC_ENTRY(SetThreadActivity), // 0x32
        SVC_ENTRY(GetThreadContext3), // 0x33
        SVC_ENTRY(WaitForAddress), // 0x34
        SVC_ENTRY(SignalToAddress), // 0x35
        SVC_NONE, // 0x36
        SVC_NONE, // 0x37
        SVC_NONE, // 0x38
        SVC_NONE, // 0x39
        SVC_NONE, // 0x3A
        SVC_NONE, // 0x3B
        SVC_NONE, // 0x3C
        SVC_NONE, // 0x3D
        SVC_NONE, // 0x3E
        SVC_NONE, // 0x3F
        SVC_NONE, // 0x40
        SVC_NONE, // 0x41
        SVC_NONE, // 0x42
        SVC_NONE, // 0x43
        SVC_NONE, // 0x44
        SVC_NONE, // 0x45
        SVC_NONE, // 0x46
        SVC_NONE, // 0x47
        SVC_NONE, // 0x48
        SVC_NONE, // 0x49
        SVC_NONE, // 0x4A
        SVC_NONE, // 0x4B
        SVC_NONE, // 0x4C
        SVC_NONE, // 0x4D
        SVC_NONE, // 0x4E
        SVC_NONE, // 0x4F
        SVC_NONE, // 0x50
        SVC_NONE, // 0x51
        SVC_NONE, // 0x52
        SVC_NONE, // 0x53
        SVC_NONE, // 0x54
        SVC_NONE, // 0x55
        SVC_NONE, // 0x56
        SVC_NONE, // 0x57
        SVC_NONE, // 0x58
        SVC_NONE, // 0x59
        SVC_NONE, // 0x5A
        SVC_NONE, // 0x5B
        SVC_NONE, // 0x5C
        SVC_NONE, // 0x5D
        SVC_NONE, // 0x5E
        SVC_NONE, // 0x5F
        SVC_NONE, // 0x60
        SVC_NONE, // 0x61
        SVC_NONE, // 0x62
        SVC_NONE, // 0x63
        SVC_NONE, // 0x64
        SVC_NONE, // 0x65
        SVC_NONE, // 0x66
        SVC_NONE, // 0x67
        SVC_NONE, // 0x68
        SVC_NONE, // 0x69
        SVC_NONE, // 0x6A
        SVC_NONE, // 0x6B
        SVC_NONE, // 0x6C
        SVC_NONE, // 0x6D
        SVC_NONE, // 0x6E
        SVC_NONE, // 0x6F
        SVC_NONE, // 0x70
        SVC_NONE, // 0x71
        SVC_NONE, // 0x72
        SVC_NONE, // 0x73
        SVC_NONE, // 0x74
        SVC_NONE, // 0x75
        SVC_NONE, // 0x76
        SVC_NONE, // 0x77
        SVC_NONE, // 0x78
        SVC_NONE, // 0x79
        SVC_NONE, // 0x7A
        SVC_NONE, // 0x7B
        SVC_NONE, // 0x7C
        SVC_NONE, // 0x7D
        SVC_NONE, // 0x7E
        SVC_NONE // 0x7F
    };

    #undef SVC_NONE
    #undef SVC_STRINGIFY
    #undef SVC_ENTRY
}
