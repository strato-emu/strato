// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <services/account/IAccountServiceForApplication.h>
#include <services/am/storage/IStorage.h>
#include "ILibraryAppletAccessor.h"

namespace skyline::service::am {
    ILibraryAppletAccessor::ILibraryAppletAccessor(const DeviceState &state, ServiceManager &manager) : stateChangeEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager) {}

    Result ILibraryAppletAccessor::GetAppletStateChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        stateChangeEvent->Signal();

        KHandle handle{state.process->InsertItem(stateChangeEvent)};
        state.logger->Debug("Applet State Change Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result ILibraryAppletAccessor::Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ILibraryAppletAccessor::GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ILibraryAppletAccessor::PushInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ILibraryAppletAccessor::PopOutData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 LaunchParameterMagic{0xC79497CA}; //!< The magic of the application launch parameters
        constexpr size_t LaunchParameterSize{0x88}; //!< The size of the launch parameter IStorage

        auto storageService{std::make_shared<IStorage>(state, manager, LaunchParameterSize)};

        storageService->Push<u32>(LaunchParameterMagic);
        storageService->Push<u32>(1);
        storageService->Push(constant::DefaultUserId);

        manager.RegisterService(storageService, session, response);
        return {};
    }
}
