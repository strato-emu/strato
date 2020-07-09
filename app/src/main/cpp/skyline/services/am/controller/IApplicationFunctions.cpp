// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/account/IAccountServiceForApplication.h>
#include <services/am/storage/IStorage.h>
#include "IApplicationFunctions.h"

namespace skyline::service::am {
    IApplicationFunctions::IApplicationFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IApplicationFunctions, "am:IApplicationFunctions", {
        {0x1, SFUNC(IApplicationFunctions::PopLaunchParameter)},
        {0x14, SFUNC(IApplicationFunctions::EnsureSaveData)},
        {0x15, SFUNC(IApplicationFunctions::GetDesiredLanguage)},
        {0x28, SFUNC(IApplicationFunctions::NotifyRunning)},
        {0x42, SFUNC(IApplicationFunctions::InitializeGamePlayRecording)},
        {0x43, SFUNC(IApplicationFunctions::SetGamePlayRecordingState)},
    }) {}

    void IApplicationFunctions::PopLaunchParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 LaunchParameterMagic = 0xc79497ca; //!< This is the magic of the application launch parameters
        constexpr size_t LaunchParameterSize = 0x88; //!< This is the size of the launch parameter IStorage

        auto storageService = std::make_shared<IStorage>(state, manager, LaunchParameterSize);

        storageService->Push<u32>(LaunchParameterMagic);
        storageService->Push<u32>(1);
        storageService->Push(constant::DefaultUserId);

        manager.RegisterService(storageService, session, response);
    }

    void IApplicationFunctions::EnsureSaveData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(0);
    }

    void IApplicationFunctions::GetDesiredLanguage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(util::MakeMagic<u64>("en-US"));
    }

    void IApplicationFunctions::NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(1);
    }

    void IApplicationFunctions::InitializeGamePlayRecording(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void IApplicationFunctions::SetGamePlayRecordingState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
