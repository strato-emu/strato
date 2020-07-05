// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <kernel/types/KProcess.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include "IApplicationDisplayService.h"
#include "ISystemDisplayService.h"
#include "IManagerDisplayService.h"

namespace skyline::service::visrv {
    IApplicationDisplayService::IApplicationDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, Service::visrv_IApplicationDisplayService, "visrv:IApplicationDisplayService", {
        {0x64, SFUNC(IApplicationDisplayService::GetRelayService)},
        {0x65, SFUNC(IApplicationDisplayService::GetSystemDisplayService)},
        {0x66, SFUNC(IApplicationDisplayService::GetManagerDisplayService)},
        {0x67, SFUNC(IApplicationDisplayService::GetIndirectDisplayTransactionService)},
        {0x3F2, SFUNC(IApplicationDisplayService::OpenDisplay)},
        {0x3FC, SFUNC(IApplicationDisplayService::CloseDisplay)},
        {0x7E4, SFUNC(IApplicationDisplayService::OpenLayer)},
        {0x7E5, SFUNC(IApplicationDisplayService::CloseLayer)},
        {0x7EE, SFUNC(IDisplayService::CreateStrayLayer)},
        {0x7EF, SFUNC(IDisplayService::DestroyStrayLayer)},
        {0x835, SFUNC(IApplicationDisplayService::SetLayerScalingMode)},
        {0x1452, SFUNC(IApplicationDisplayService::GetDisplayVsyncEvent)},
    }) {}

    void IApplicationDisplayService::GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(hosbinder::IHOSBinderDriver), session, response, false);
    }

    void IApplicationDisplayService::GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(hosbinder::IHOSBinderDriver), session, response, false);
    }

    void IApplicationDisplayService::GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISystemDisplayService), session, response);
    }

    void IApplicationDisplayService::GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IManagerDisplayService), session, response);
    }

    void IApplicationDisplayService::OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string displayName(request.PopString());
        state.logger->Debug("Setting display as: {}", displayName);
        state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>(Service::hosbinder_IHOSBinderDriver)->SetDisplay(displayName);

        response.Push<u64>(0); // There's only one display
    }

    void IApplicationDisplayService::CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Closing the display");
        state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>(Service::hosbinder_IHOSBinderDriver)->CloseDisplay();
    }

    void IApplicationDisplayService::OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            char displayName[0x40];
            u64 layerId;
            u64 userId;
        } input = request.Pop<InputStruct>();
        state.logger->Debug("Opening Layer: Display Name: {}, Layer ID: {}, User ID: {}", input.displayName, input.layerId, input.userId);

        std::string name(input.displayName);

        Parcel parcel(state);
        LayerParcel data{
            .type = 0x2,
            .pid = 0,
            .bufferId = 0, // As we only have one layer and buffer
            .string = "dispdrv"
        };
        parcel.WriteData(data);
        parcel.objects.resize(4);

        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));
    }

    void IApplicationDisplayService::CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 layerId = request.Pop<u64>();
        state.logger->Debug("Closing Layer: {}", layerId);

        auto hosBinder = state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>(Service::hosbinder_IHOSBinderDriver);
        if (hosBinder->layerStatus == hosbinder::LayerStatus::Uninitialized)
            state.logger->Warn("The application is destroying an uninitialized layer");
        hosBinder->layerStatus = hosbinder::LayerStatus::Uninitialized;

        response.Push<u32>(constant::status::Success);
    }

    void IApplicationDisplayService::SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto scalingMode = request.Pop<u64>();
        auto layerId = request.Pop<u64>();

        state.logger->Debug("Setting Layer Scaling mode to '{}' for layer {}", scalingMode, layerId);
    }

    void IApplicationDisplayService::GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle = state.process->InsertItem(state.gpu->vsyncEvent);
        state.logger->Debug("VSync Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
    }
}
