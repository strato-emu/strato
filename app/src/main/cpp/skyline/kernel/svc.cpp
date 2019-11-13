#include "svc.h"
#include <os.h>
#include <kernel/types/KTransferMemory.h>

namespace skyline::kernel::svc {
    void SetHeapSize(DeviceState &state) {
        u32 size = state.nce->GetRegister(Wreg::W1);
        std::shared_ptr<type::KPrivateMemory> heap;
        try {
            heap = state.thisProcess->memoryRegionMap.at(memory::Region::Heap);
            heap->Resize(size, true); // This can fail due to not enough space to resize
        } catch (const exception &) {
            state.logger->Warn("svcSetHeapSize is falling back to recreating memory");
            state.thisProcess->UnmapPrivateRegion(memory::Region::Heap);
            heap = state.thisProcess->MapPrivateRegion(constant::HeapAddr, size, {true, true, false}, memory::Type::Heap, memory::Region::Heap).item;
        }
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Xreg::X1, heap->address);
        state.logger->Debug("svcSetHeapSize allocated at 0x{:X} for 0x{:X} bytes", heap->address, heap->size);
    }

    void SetMemoryAttribute(DeviceState &state) {
        u64 addr = state.nce->GetRegister(Xreg::X0);
        u64 size = state.nce->GetRegister(Xreg::X1);
        bool isUncached = (state.nce->GetRegister(Wreg::W2) == 8) && (state.nce->GetRegister(Wreg::W3) == 8);
        bool found = false;
        for (const auto&[address, region] : state.thisProcess->memoryMap) {
            if (addr >= address && addr < (address + region->size)) {
                bool subFound = false;
                for (auto &subregion : region->regionInfoVec) {
                    if ((address >= subregion.address) && (address < (subregion.address + subregion.size)))
                        subregion.isUncached = isUncached;
                    subFound = true;
                    break;
                }
                if (!subFound)
                    region->regionInfoVec.push_back(memory::RegionInfo{.address=addr, .size=size, .isUncached=isUncached});
                found = true;
                break;
            }
        }
        state.logger->Debug("svcSetMemoryAttribute set caching to {} at 0x{:X} for 0x{:X} bytes", !isUncached, addr, size);
        state.nce->SetRegister(Wreg::W0, found ? constant::status::Success : constant::status::InvAddress);
    }

    void QueryMemory(DeviceState &state) {
        memory::MemoryInfo memInf;
        u64 addr = state.nce->GetRegister(Xreg::X2);
        bool found = false;
        for (const auto&[address, region] : state.thisProcess->memoryMap) {
            if (addr >= address && addr < (address + region->size)) {
                memInf = region->GetInfo(addr);
                found = true;
                break;
            }
        }
        if (!found) {
            for (const auto &object : state.thisProcess->handleTable) {
                if (object.second->objectType == type::KType::KSharedMemory) {
                    const auto &mem = state.thisProcess->GetHandle<type::KSharedMemory>(object.first);
                    if (mem->procInfMap.count(state.thisProcess->mainThread)) {
                        const auto &map = mem->procInfMap.at(state.thisProcess->mainThread);
                        if (addr >= map.address && addr < (map.address + map.size)) {
                            memInf = mem->GetInfo(state.thisProcess->mainThread);
                            found = true;
                            break;
                        }
                    }
                } else if (object.second->objectType == type::KType::KTransferMemory) {
                    const auto &mem = state.thisProcess->GetHandle<type::KTransferMemory>(object.first);
                    if (addr >= mem->cAddress && addr < (mem->cAddress + mem->cSize)) {
                        memInf = mem->GetInfo();
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                memInf = {
                    .baseAddress = constant::BaseEnd,
                    .size = static_cast<u64>(-constant::BaseEnd + 1),
                    .type = static_cast<u64>(memory::Type::Unmapped)
                };
                state.logger->Warn("Cannot find block of address: 0x{:X}", addr);
            }
        }
        state.thisProcess->WriteMemory<memory::MemoryInfo>(memInf, state.nce->GetRegister(Xreg::X0));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void ExitProcess(DeviceState &state) {
        state.os->KillThread(state.thisProcess->mainThread);
    }

    void CreateThread(DeviceState &state) {
        // TODO: Support Core Mask potentially
        auto thread = state.thisProcess->CreateThread(state.nce->GetRegister(Xreg::X1), state.nce->GetRegister(Xreg::X2), state.nce->GetRegister(Xreg::X3), static_cast<u8>(state.nce->GetRegister(Wreg::W4)));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Wreg::W1, thread->handle);
        state.logger->Info("Creating a thread: {}", thread->handle);
    }

    void StartThread(DeviceState &state) {
        auto &object = state.thisProcess->handleTable.at(static_cast<const unsigned int &>(state.nce->GetRegister(Wreg::W0)));
        if (object->objectType == type::KType::KThread)
            std::static_pointer_cast<type::KThread>(object)->Start();
        else
            throw exception("StartThread was called on a non-KThread object");
    }

    void ExitThread(DeviceState &state) {
        state.os->KillThread(state.thisThread->pid);
    }

    void SleepThread(DeviceState &state) {
        auto in = state.nce->GetRegister(Xreg::X0);
        switch (in) {
            case 0:
            case 1:
            case 2:
                state.thisThread->status = type::KThread::Status::Runnable; // Will cause the application to awaken on the next iteration of the main loop
            default:
                state.thisThread->timeout = GetCurrTimeNs() + in;
                state.thisThread->status = type::KThread::Status::Sleeping;
        }
    }

    void GetThreadPriority(DeviceState &state) {
        state.nce->SetRegister(Wreg::W1, state.thisProcess->GetHandle<type::KThread>(static_cast<handle_t>(state.nce->GetRegister(Wreg::W0)))->priority);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void SetThreadPriority(DeviceState &state) {
        state.thisProcess->GetHandle<type::KThread>(static_cast<handle_t>(state.nce->GetRegister(Wreg::W0)))->Start();
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void MapSharedMemory(DeviceState &state) {
        auto object = state.thisProcess->GetHandle<type::KSharedMemory>(static_cast<handle_t>(state.nce->GetRegister(Wreg::W0)));
        object->Map(state.nce->GetRegister(Xreg::X1), state.nce->GetRegister(Xreg::X2), state.thisProcess->mainThread);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void CloseHandle(DeviceState &state) {
        auto handle = static_cast<handle_t>(state.nce->GetRegister(Wreg::W0));
        state.logger->Debug("Closing handle: 0x{:X}", handle);
        auto &object = state.thisProcess->handleTable.at(handle);
        switch (object->objectType) {
            case (type::KType::KThread):
                state.os->KillThread(std::static_pointer_cast<type::KThread>(object)->pid);
                break;
            case (type::KType::KProcess):
                state.os->KillThread(std::static_pointer_cast<type::KProcess>(object)->mainThread);
                break;
            default:
                state.thisProcess->handleTable.erase(handle);
        }
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void CreateTransferMemory(DeviceState &state) {
        u64 address = state.nce->GetRegister(Xreg::X1);
        u64 size = state.nce->GetRegister(Xreg::X2);
        u32 perms = state.nce->GetRegister(Wreg::W3);
        auto shmem = state.thisProcess->NewHandle<type::KTransferMemory>(state.thisProcess->mainThread, address, size, *reinterpret_cast<memory::Permission *>(&perms));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Wreg::W1, shmem.handle);
    }

    void ResetSignal(DeviceState &state) {
        try {
            state.thisProcess->GetHandle<type::KEvent>(state.nce->GetRegister(Wreg::W0))->ResetSignal();
        } catch (const exception &) {
            state.thisProcess->GetHandle<type::KProcess>(state.nce->GetRegister(Wreg::W0))->ResetSignal();
        }
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void WaitSynchronization(DeviceState &state) {
        auto numHandles = state.nce->GetRegister(Wreg::W2);
        if (numHandles > constant::MaxSyncHandles) {
            state.nce->SetRegister(Wreg::W0, constant::status::MaxHandles);
            return;
        }
        std::vector<handle_t> waitHandles(numHandles);
        state.thisProcess->ReadMemory(waitHandles.data(), state.nce->GetRegister(Xreg::X1), numHandles * sizeof(handle_t));
        std::string handleStr;
        for (const auto &handle : waitHandles) {
            handleStr += fmt::format("* 0x{:X}\n", handle);
            auto object = state.thisProcess->handleTable.at(handle);
            switch (object->objectType) {
                case type::KType::KProcess:
                case type::KType::KThread:
                case type::KType::KEvent:
                case type::KType::KSession:
                    break;
                default:
                    state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
                    return;
            }
            auto syncObject = std::static_pointer_cast<type::KSyncObject>(object);
            if (syncObject->signalled) {
                state.logger->Debug("Found signalled handle: 0x{:X}", handle);
                state.nce->SetRegister(Wreg::W0, constant::status::Success);
                state.nce->SetRegister(Wreg::W1, handle);
                return;
            }
            state.thisThread->waitObjects.push_back(syncObject);
            syncObject->waitThreads.emplace_back(state.thisThread->pid, handle);
        }
        state.logger->Debug("Waiting on handles:\n{}Timeout: 0x{:X} ns", handleStr, state.nce->GetRegister(Xreg::X3));
        if (state.nce->GetRegister(Xreg::X3) != std::numeric_limits<u64>::max())
            state.thisThread->timeout = GetCurrTimeNs() + state.nce->GetRegister(Xreg::X3);
        else
            state.thisThread->timeout = 0;
        state.thisThread->status = type::KThread::Status::WaitSync;
    }

    void GetSystemTick(DeviceState &state) {
        u64 tick{};
        asm("STR X1, [SP, #-16]!\n\t"
            "MRS %0, CNTVCT_EL0\n\t"
            "MOV X1, #0xF800\n\t"
            "MOVK X1, #0x124, lsl #16\n\t"
            "MUL %0, %0, X1\n\t"
            "MRS X1, CNTFRQ_EL0\n\t"
            "UDIV %0, %0, X1\n\t"
            "LDR X1, [SP], #16" : "=r"(tick));
        state.nce->SetRegister(Xreg::X0, tick);
    }

    void ArbitrateLock(DeviceState &state) {
        if (state.nce->GetRegister(Wreg::W2) != state.thisThread->handle)
            throw exception("A process requested locking a thread on behalf of another process");
        state.thisProcess->MutexLock(state.nce->GetRegister(Xreg::X1));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void ArbitrateUnlock(DeviceState &state) {
        state.thisProcess->MutexUnlock(state.nce->GetRegister(Xreg::X0));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void WaitProcessWideKeyAtomic(DeviceState &state) {
        auto mtxAddr = state.nce->GetRegister(Xreg::X0);
        if (state.nce->GetRegister(Wreg::W2) != state.thisThread->handle)
            throw exception("svcWaitProcessWideKeyAtomic was called on behalf of another thread");
        state.thisProcess->MutexUnlock(mtxAddr);
        auto &cvarVec = state.thisProcess->condVarMap[state.nce->GetRegister(Xreg::X1)];
        for (auto thread = cvarVec.begin();; thread++) {
            if ((*thread)->priority < state.thisThread->priority) {
                cvarVec.insert(thread, state.thisThread);
                break;
            } else if (thread + 1 == cvarVec.end()) {
                cvarVec.push_back(state.thisThread);
                break;
            }
        }
        state.thisThread->status = type::KThread::Status::WaitCondVar;
        state.thisThread->timeout = GetCurrTimeNs() + state.nce->GetRegister(Xreg::X3);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void SignalProcessWideKey(DeviceState &state) {
        auto address = state.nce->GetRegister(Xreg::X0);
        auto count = state.nce->GetRegister(Wreg::W1);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        if (!state.thisProcess->condVarMap.count(address))
            return; // No threads to awaken
        auto &cvarVec = state.thisProcess->condVarMap[address];
        count = std::min(count, static_cast<u32>(cvarVec.size()));
        for (uint index = 0; index < count; index++)
            cvarVec[index]->status = type::KThread::Status::Runnable;
        cvarVec.erase(cvarVec.begin(), cvarVec.begin() + count);
        if (cvarVec.empty())
            state.thisProcess->condVarMap.erase(address);
    }

    void ConnectToNamedPort(DeviceState &state) {
        char port[constant::PortSize + 1]{0}; // +1 so string will always be null terminated
        state.os->thisProcess->ReadMemory(port, state.nce->GetRegister(Xreg::X1), constant::PortSize);
        if (std::strcmp(port, "sm:") == 0)
            state.nce->SetRegister(Wreg::W1, state.os->serviceManager.NewSession(service::Service::sm));
        else
            throw exception("svcConnectToNamedPort tried connecting to invalid port: \"{}\"", port);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void SendSyncRequest(DeviceState &state) {
        state.os->serviceManager.SyncRequestHandler(static_cast<handle_t>(state.nce->GetRegister(Xreg::X0)));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void GetThreadId(DeviceState &state) {
        pid_t pid{};
        if (state.nce->GetRegister(Wreg::W1) != constant::ThreadSelf) {
            handle_t thread = state.nce->GetRegister(Wreg::W1);
            pid = state.thisProcess->GetHandle<type::KThread>(thread)->pid;
        } else
            pid = state.thisThread->pid;
        state.nce->SetRegister(Xreg::X1, static_cast<u64>(pid));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void OutputDebugString(DeviceState &state) {
        std::string debug(state.nce->GetRegister(Xreg::X1), '\0');
        state.os->thisProcess->ReadMemory(debug.data(), state.nce->GetRegister(Xreg::X0), state.nce->GetRegister(Xreg::X1));
        state.logger->Info("Debug Output: {}", debug);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void GetInfo(DeviceState &state) {
        state.logger->Debug("svcGetInfo called with ID0: {}, ID1: {}", state.nce->GetRegister(Wreg::W1), state.nce->GetRegister(Xreg::X3));
        switch (state.nce->GetRegister(Wreg::W1)) {
            case constant::infoState::AllowedCpuIdBitmask:
            case constant::infoState::AllowedThreadPriorityMask:
            case constant::infoState::IsCurrentProcessBeingDebugged:
            case constant::infoState::TitleId:
            case constant::infoState::PrivilegedProcessId:
                state.nce->SetRegister(Xreg::X1, 0);
                break;
            case constant::infoState::AliasRegionBaseAddr:
                state.nce->SetRegister(Xreg::X1, constant::MapAddr);
                break;
            case constant::infoState::AliasRegionSize:
                state.nce->SetRegister(Xreg::X1, constant::MapSize);
                break;
            case constant::infoState::HeapRegionBaseAddr:
                state.nce->SetRegister(Xreg::X1, state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address);
                break;
            case constant::infoState::HeapRegionSize:
                state.nce->SetRegister(Xreg::X1, state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->size);
                break;
            case constant::infoState::TotalMemoryAvailable:
                state.nce->SetRegister(Xreg::X1, constant::TotalPhyMem);
                break;
            case constant::infoState::TotalMemoryUsage:
                state.nce->SetRegister(Xreg::X1, state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address + state.thisProcess->mainThreadStackSz + state.thisProcess->GetProgramSize());
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                state.nce->SetRegister(Xreg::X1, constant::BaseAddr);
                break;
            case constant::infoState::AddressSpaceSize:
                state.nce->SetRegister(Xreg::X1, constant::BaseSize);
                break;
            case constant::infoState::StackRegionBaseAddr:
                state.nce->SetRegister(Xreg::X1, state.thisThread->stackTop);
                break;
            case constant::infoState::StackRegionSize:
                state.nce->SetRegister(Xreg::X1, state.thisProcess->mainThreadStackSz);
                break;
            case constant::infoState::PersonalMmHeapSize:
                state.nce->SetRegister(Xreg::X1, constant::TotalPhyMem);
                break;
            case constant::infoState::PersonalMmHeapUsage:
                state.nce->SetRegister(Xreg::X1, state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address + state.thisProcess->mainThreadStackSz);
                break;
            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                state.nce->SetRegister(Xreg::X1, constant::TotalPhyMem); // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;
            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                state.nce->SetRegister(Xreg::X1, state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address + state.thisProcess->mainThreadStackSz); // TODO: Same as above
                break;
            case constant::infoState::UserExceptionContextAddr:
                state.nce->SetRegister(Xreg::X1, state.thisProcess->tlsPages[0]->Get(0));
                break;
            default:
                state.logger->Warn("Unimplemented svcGetInfo with ID0: {}, ID1: {}", state.nce->GetRegister(Wreg::W1), state.nce->GetRegister(Xreg::X3));
                state.nce->SetRegister(Wreg::W0, constant::status::Unimpl);
                return;
        }
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }
}
