// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <gpu.h>
#include <kernel/types/KProcess.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include <services/hosbinder/GraphicBufferProducer.h>
#include "IApplicationDisplayService.h"
#include "ISystemDisplayService.h"
#include "IManagerDisplayService.h"

namespace skyline::service::visrv {
    IApplicationDisplayService::IApplicationDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager) {}

    Result IApplicationDisplayService::GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(hosbinder::IHOSBinderDriver), session, response);
        return {};
    }

    Result IApplicationDisplayService::GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(hosbinder::IHOSBinderDriver), session, response);
        return {};
    }

    Result IApplicationDisplayService::GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISystemDisplayService), session, response);
        return {};
    }

    Result IApplicationDisplayService::GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IManagerDisplayService), session, response);
        return {};
    }

    Result IApplicationDisplayService::OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string displayName(request.PopString());
        state.logger->Debug("Setting display as: {}", displayName);

        auto producer{hosbinder::producer.lock()};
        producer->SetDisplay(displayName);

        response.Push<u64>(0); // There's only one display
        return {};
    }

    Result IApplicationDisplayService::CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Closing the display");
        auto producer{hosbinder::producer.lock()};
        producer->CloseDisplay();
        return {};
    }

    Result IApplicationDisplayService::OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
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
        parcel.Push(data);
        parcel.objects.resize(4);

        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));
        return {};
    }

    Result IApplicationDisplayService::CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 layerId{request.Pop<u64>()};
        state.logger->Debug("Closing Layer: {}", layerId);

        auto producer{hosbinder::producer.lock()};
        if (producer->layerStatus == hosbinder::LayerStatus::Uninitialized)
            state.logger->Warn("The application is destroying an uninitialized layer");
        producer->layerStatus = hosbinder::LayerStatus::Uninitialized;

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
        state.logger->Debug("VSync Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }
}
