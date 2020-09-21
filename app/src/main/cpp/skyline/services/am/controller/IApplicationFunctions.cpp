// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <services/account/IAccountServiceForApplication.h>
#include <services/am/storage/IStorage.h>
#include "IApplicationFunctions.h"

namespace skyline::service::am {
    IApplicationFunctions::IApplicationFunctions(const DeviceState &state, ServiceManager &manager) : gpuErrorEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager) {}

    Result IApplicationFunctions::PopLaunchParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 LaunchParameterMagic = 0xC79497CA; //!< This is the magic of the application launch parameters
        constexpr size_t LaunchParameterSize = 0x88; //!< This is the size of the launch parameter IStorage

        auto storageService = std::make_shared<IStorage>(state, manager, LaunchParameterSize);

        storageService->Push<u32>(LaunchParameterMagic);
        storageService->Push<u32>(1);
        storageService->Push(constant::DefaultUserId);

        manager.RegisterService(storageService, session, response);
        return {};
    }

    Result IApplicationFunctions::EnsureSaveData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(0);
        return {};
    }

    Result IApplicationFunctions::GetDesiredLanguage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(util::MakeMagic<u64>("en-US"));
        return {};
    }

    Result IApplicationFunctions::NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(1);
        return {};
    }

    Result IApplicationFunctions::GetPseudoDeviceId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(0L);
        response.Push<u64>(0L);
        return {};
    }

    Result IApplicationFunctions::InitializeGamePlayRecording(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IApplicationFunctions::SetGamePlayRecordingState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IApplicationFunctions::GetGpuErrorDetectedSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = state.process->InsertItem(gpuErrorEvent);
        state.logger->Debug("GPU Error Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }
}
