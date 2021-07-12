// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2019 Ryujinx Team and Contributors

#include <gpu.h>
#include <kernel/types/KProcess.h>
#include <services/serviceman.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include "IApplicationDisplayService.h"
#include "ISystemDisplayService.h"
#include "IManagerDisplayService.h"
#include "results.h"

namespace skyline::service::visrv {
    IApplicationDisplayService::IApplicationDisplayService(const DeviceState &state, ServiceManager &manager, PrivilegeLevel level) : level(level), IDisplayService(state, manager) {}

    Result IApplicationDisplayService::GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(hosbinder, session, response);
        return {};
    }

    Result IApplicationDisplayService::GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (level < PrivilegeLevel::System)
            return result::IllegalOperation;
        manager.RegisterService(hosbinder, session, response);
        return {};
    }

    Result IApplicationDisplayService::GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (level < PrivilegeLevel::System)
            return result::IllegalOperation;
        manager.RegisterService(SRVREG(ISystemDisplayService), session, response);
        return {};
    }

    Result IApplicationDisplayService::GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (level < PrivilegeLevel::Manager)
            return result::IllegalOperation;
        manager.RegisterService(SRVREG(IManagerDisplayService), session, response);
        return {};
    }

    Result IApplicationDisplayService::OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto displayName(request.PopString());
        state.logger->Debug("Opening display: {}", displayName);
        response.Push(hosbinder->OpenDisplay(displayName));
        return {};
    }

    Result IApplicationDisplayService::CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto displayId{request.Pop<hosbinder::DisplayId>()};
        state.logger->Debug("Closing display: {}", hosbinder::ToString(displayId));
        hosbinder->CloseDisplay(displayId);
        return {};
    }

    Result IApplicationDisplayService::OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto displayName{request.PopString(0x40)};
        auto layerId{request.Pop<u64>()};
        state.logger->Debug("Opening layer #{} on display: {}", layerId, displayName);

        auto displayId{hosbinder->OpenDisplay(displayName)};
        auto parcel{hosbinder->OpenLayer(displayId, layerId)};
        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));

        return {};
    }

    Result IApplicationDisplayService::CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 layerId{request.Pop<u64>()};
        state.logger->Debug("Closing layer #{}", layerId);
        hosbinder->CloseLayer(layerId);
        return {};
    }

    Result IApplicationDisplayService::SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto scalingMode{request.Pop<u64>()};
        auto layerId{request.Pop<u64>()};
        state.logger->Debug("Setting Layer Scaling mode to '{}' for layer {}", scalingMode, layerId);
        return {};
    }

    Result IApplicationDisplayService::GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(state.gpu->presentation.vsyncEvent)};
        state.logger->Debug("V-Sync Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IApplicationDisplayService::ConvertScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 inScalingMode{request.Pop<u32>()};

        std::array<ScalingMode, 5> scalingModeLut{
            ScalingMode::None,
            ScalingMode::Freeze,
            ScalingMode::ScaleToLayer,
            ScalingMode::ScaleAndCrop,
            ScalingMode::PreserveAspectRatio,
        };

        if (scalingModeLut.size() <= inScalingMode)
            return result::InvalidArgument;

        auto scalingMode{scalingModeLut[inScalingMode]};
        if (scalingMode != ScalingMode::ScaleToLayer && scalingMode != ScalingMode::PreserveAspectRatio)
            return result::IllegalOperation;

        response.Push(scalingMode);

        return {};
    }
}
