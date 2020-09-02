// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <services/account/IAccountServiceForApplication.h>
#include <services/am/storage/IStorage.h>
#include "ILibraryAppletAccessor.h"

namespace skyline::service::am {
    ILibraryAppletAccessor::ILibraryAppletAccessor(const DeviceState &state, ServiceManager &manager) : stateChangeEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager, {
        {0x0, SFUNC(ILibraryAppletAccessor::GetAppletStateChangedEvent)},
        {0xA, SFUNC(ILibraryAppletAccessor::Start)},
        {0x1E, SFUNC(ILibraryAppletAccessor::GetResult)},
        {0x64, SFUNC(ILibraryAppletAccessor::PushInData)},
        {0x65, SFUNC(ILibraryAppletAccessor::PopOutData)},
    }) {}

    void ILibraryAppletAccessor::GetAppletStateChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        stateChangeEvent->Signal();

        KHandle handle = state.process->InsertItem(stateChangeEvent);
        state.logger->Debug("Applet State Change Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
    }

    void ILibraryAppletAccessor::Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ILibraryAppletAccessor::GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ILibraryAppletAccessor::PushInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ILibraryAppletAccessor::PopOutData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 LaunchParameterMagic = 0xC79497CA; //!< This is the magic of the application launch parameters
        constexpr size_t LaunchParameterSize = 0x88; //!< This is the size of the launch parameter IStorage

        auto storageService = std::make_shared<IStorage>(state, manager, LaunchParameterSize);

        storageService->Push<u32>(LaunchParameterMagic);
        storageService->Push<u32>(1);
        storageService->Push(constant::DefaultUserId);

        manager.RegisterService(storageService, session, response);
    }
}
