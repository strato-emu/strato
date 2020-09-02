// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include <services/hosbinder/display.h>
#include "IDisplayService.h"

namespace skyline::service::visrv {
    IDisplayService::IDisplayService(const DeviceState &state, ServiceManager &manager, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : BaseService(state, manager, vTable) {}

    void IDisplayService::CreateStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u64>();
        auto displayId = request.Pop<u64>();

        state.logger->Debug("Creating Stray Layer on Display: {}", displayId);

        auto hosBinder = state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>("dispdrv");
        if (hosBinder->layerStatus == hosbinder::LayerStatus::Stray)
            throw exception("The application is creating more than one stray layer");
        hosBinder->layerStatus = hosbinder::LayerStatus::Stray;
        response.Push<u64>(0); // There's only one layer

        Parcel parcel(state);
        LayerParcel data{
            .type = 0x2,
            .pid = 0,
            .bufferId = 0, // As we only have one layer and buffer
            .string = "dispdrv"
        };
        parcel.WriteData(data);

        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));
    }

    void IDisplayService::DestroyStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId = request.Pop<u64>();
        state.logger->Debug("Destroying Stray Layer: {}", layerId);

        auto hosBinder = state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>("dispdrv");
        if (hosBinder->layerStatus == hosbinder::LayerStatus::Uninitialized)
            state.logger->Warn("The application is destroying an uninitialized layer");
        hosBinder->layerStatus = hosbinder::LayerStatus::Uninitialized;
    }
}
