// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/serviceman.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include "IDisplayService.h"

namespace skyline::service::visrv {
    IDisplayService::IDisplayService(const DeviceState &state, ServiceManager &manager) : hosbinder(manager.CreateOrGetService<hosbinder::IHOSBinderDriver>("dispdrv")), BaseService(state, manager) {}

    Result IDisplayService::CreateStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u64>(); // VI Layer flags
        auto displayId{request.Pop<hosbinder::DisplayId>()};

        auto layerId{hosbinder->CreateLayer(displayId)};
        response.Push(layerId);

        state.logger->Debug("Creating Stray Layer #{} on Display: {}", layerId, hosbinder::ToString(displayId));

        auto parcel{hosbinder->OpenLayer(displayId, layerId)};
        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));

        return {};
    }

    Result IDisplayService::DestroyStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId{request.Pop<u64>()};
        state.logger->Debug("Destroying Stray Layer #{}", layerId);

        hosbinder->CloseLayer(layerId);

        return {};
    }
}
