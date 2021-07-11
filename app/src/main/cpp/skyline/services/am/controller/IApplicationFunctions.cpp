// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/uuid.h>
#include <mbedtls/sha1.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include <services/account/IAccountServiceForApplication.h>
#include <services/am/storage/IStorage.h>
#include "IApplicationFunctions.h"

namespace skyline::service::am {
    IApplicationFunctions::IApplicationFunctions(const DeviceState &state, ServiceManager &manager) : gpuErrorEvent(std::make_shared<type::KEvent>(state, false)), BaseService(state, manager) {}

    Result IApplicationFunctions::PopLaunchParameter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 LaunchParameterMagic{0xC79497CA}; //!< The magic of the application launch parameters
        constexpr size_t LaunchParameterSize{0x88}; //!< The size of the launch parameter IStorage

        auto storageService{std::make_shared<IStorage>(state, manager, LaunchParameterSize)};

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
        auto seedForPseudoDeviceId{state.loader->nacp->nacpContents.seedForPseudoDeviceId};
        std::array<u8, 20> hashBuf{};

        // On HOS the seed from control.ncap is hashed together with the device specific device ID seed
        // for us it's enough to just hash the seed from control.nacp as it provides the same guarantees
        if (int err{mbedtls_sha1(seedForPseudoDeviceId.data(), seedForPseudoDeviceId.size(), hashBuf.data())}; err < 0)
            throw exception("Failed to hash device ID, err: {}", err);

        response.Push<UUID>(UUID::GenerateUuidV5(hashBuf));
        return {};
    }

    Result IApplicationFunctions::InitializeGamePlayRecording(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IApplicationFunctions::SetGamePlayRecordingState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IApplicationFunctions::InitializeApplicationCopyrightFrameBuffer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        i32 width{request.Pop<i32>()};
        i32 height{request.Pop<i32>()};
        u64 transferMemorySize{request.Pop<u64>()};

        constexpr i32 MaximumFbWidth{1280};
        constexpr i32 MaximumFbHeight{720};
        constexpr u64 RequiredFbAlignment{0x40000};

        if (width > MaximumFbWidth || height > MaximumFbHeight || !util::IsAligned(transferMemorySize, RequiredFbAlignment))
            return result::InvalidParameters;

        state.logger->Debug("Dimensions: ({}, {}) Transfer Memory Size: {}", width, height, transferMemorySize);

        return {};
    }

    Result IApplicationFunctions::SetApplicationCopyrightImage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        i32 x{request.Pop<i32>()};
        i32 y{request.Pop<i32>()};
        i32 width{request.Pop<i32>()};
        i32 height{request.Pop<i32>()};

        enum class WindowOriginMode : i32 {
            LowerLeft,
            UpperLeft
        } originMode = request.Pop<WindowOriginMode>();

        if (y < 0 || x < 0 || width < 1 || height < 1)
            return result::InvalidParameters;

        state.logger->Debug("Position: ({}, {}) Dimensions: ({}, {}) Origin mode: {}", x, y, width, height, static_cast<i32>(originMode));
        return {};
    }

    Result IApplicationFunctions::SetApplicationCopyrightVisibility(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u8 visiblity{request.Pop<u8>()};
        state.logger->Debug("Visiblity: {}", visiblity);
        return {};
    }

    Result IApplicationFunctions::GetGpuErrorDetectedSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(gpuErrorEvent)};
        state.logger->Debug("GPU Error Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }
}
