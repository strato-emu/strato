#include "svc.h"
#include <os.h>

namespace skyline::kernel::svc {
    void SetHeapSize(DeviceState &state) {
        auto heap = state.thisProcess->MapPrivateRegion(0, state.nce->GetRegister(Wreg::W1), {true, true, false}, memory::Type::Heap, memory::Region::Heap);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Xreg::X1, heap.item->address);
        state.logger->Write(Logger::Debug, "Heap size was set to 0x{:X}", state.nce->GetRegister(Wreg::W1));
    }

    void QueryMemory(DeviceState &state) {
        memory::MemoryInfo memInf;
        u64 addr = state.nce->GetRegister(Xreg::X2);
        bool memFree = true;
        for(const auto& [address, region] : state.thisProcess->memoryMap) {
            if (addr >= address && addr < (address + region->size)) {
                memInf = region->GetInfo();
                memFree = false;
                break;
            }
        }
        if (memFree) {
            memInf = {
                .baseAddress = static_cast<u64>(static_cast<u64>(addr / PAGE_SIZE) * PAGE_SIZE),
                .size =  static_cast<u64>(-constant::BaseSize + 1),
                .type = static_cast<u64>(memory::Type::Unmapped),
                };
        }
        state.thisProcess->WriteMemory<memory::MemoryInfo>(memInf, state.nce->GetRegister(Xreg::X0));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void CreateThread(DeviceState &state) {
        // TODO: Support Core Mask potentially
        auto thread = state.thisProcess->CreateThread(state.nce->GetRegister(Xreg::X1), state.nce->GetRegister(Xreg::X2), state.nce->GetRegister(Xreg::X3), static_cast<u8>(state.nce->GetRegister(Wreg::W4)));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Wreg::W1, thread->handle);
        state.logger->Write(Logger::Info, "Creating a thread: {}", thread->handle);
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
        switch(in) {
            case 0:
            case 1:
            case 2:
                state.thisThread->status = type::KThread::ThreadStatus::Runnable; // Will cause the application to awaken on the next iteration of the main loop
            default:
                state.thisThread->timeoutEnd = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() + in;
                state.thisThread->status = type::KThread::ThreadStatus::Sleeping;
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
        state.logger->Write(Logger::Debug, "Closing handle: 0x{:X}", handle);
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

    void WaitSynchronization(DeviceState &state) {
        state.thisThread->timeoutEnd = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() + state.nce->GetRegister(Xreg::X3);
        auto numHandles = state.nce->GetRegister(Wreg::W2);
        if (numHandles > constant::MaxSyncHandles) {
            state.nce->SetRegister(Wreg::W0, constant::status::MaxHandles);
            return;
        }
        std::vector<handle_t> waitHandles(numHandles);
        state.thisProcess->ReadMemory(waitHandles.data(), state.nce->GetRegister(Xreg::X1), numHandles * sizeof(handle_t));
        for (const auto& handle : waitHandles) {
            auto object = state.thisProcess->handleTable.at(handle);
            switch(object->objectType) {
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
            if(syncObject->signalled) {
                state.nce->SetRegister(Wreg::W0, constant::status::Success);
                return;
            }
            state.thisThread->waitObjects.push_back(syncObject);
            syncObject->waitThreads.push_back(state.thisThread->pid);
        }
        state.thisThread->status = type::KThread::ThreadStatus::Waiting;
    }

    void ConnectToNamedPort(DeviceState &state) {
        char port[constant::PortSize + 1]{0}; // +1 so string will always be null terminated
        state.os->thisProcess->ReadMemory(port, state.nce->GetRegister(Xreg::X1), constant::PortSize);
        if (std::strcmp(port, "sm:") == 0)
            state.nce->SetRegister(Wreg::W1, state.os->serviceManager.NewSession(service::Service::sm));
        else
            throw exception(fmt::format("svcConnectToNamedPort tried connecting to invalid port: \"{}\"", port));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void SendSyncRequest(DeviceState &state) {
        state.os->serviceManager.SyncRequestHandler(static_cast<handle_t>(state.nce->GetRegister(Xreg::X0)));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void OutputDebugString(DeviceState &state) {
        std::string debug(state.nce->GetRegister(Xreg::X1), '\0');
        state.os->thisProcess->ReadMemory((void *) debug.data(), state.nce->GetRegister(Xreg::X0), state.nce->GetRegister(Xreg::X1));
        std::string::size_type pos = 0;
        while ((pos = debug.find("\r\n", pos)) != std::string::npos)
            debug.erase(pos, 2);
        state.logger->Write(Logger::Info, "svcOutputDebugString: {}", debug);
        state.nce->SetRegister(Wreg::W0, 0);
    }

    void GetInfo(DeviceState &state) {
        state.logger->Write(Logger::Debug, "svcGetInfo called with ID0: {}, ID1: {}", state.nce->GetRegister(Wreg::W1), state.nce->GetRegister(Xreg::X3));
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
                state.logger->Write(Logger::Warn, "Unimplemented svcGetInfo with ID0: {}, ID1: {}", state.nce->GetRegister(Wreg::W1), state.nce->GetRegister(Xreg::X3));
                state.nce->SetRegister(Wreg::W0, constant::status::Unimpl);
                return;
        }
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void ExitProcess(DeviceState &state) {
        state.os->KillThread(state.thisProcess->mainThread);
    }
}
