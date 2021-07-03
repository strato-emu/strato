// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/hosbinder/IHOSBinderDriver.h>
#include "IManagerDisplayService.h"

namespace skyline::service::visrv {
    IManagerDisplayService::IManagerDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager) {}

    Result IManagerDisplayService::CreateManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u64>(); // VI Layer flags
        auto displayId{request.Pop<hosbinder::DisplayId>()};

        auto layerId{hosbinder->CreateLayer(displayId)};
        state.logger->Debug("Creating Managed Layer #{} on Display: {}", layerId, hosbinder::ToString(displayId));
        response.Push(layerId);

        return {};
    }

    Result IManagerDisplayService::DestroyManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId{request.Pop<u64>()};
        state.logger->Debug("Destroying Managed Layer #{}", layerId);
        hosbinder->DestroyLayer(layerId);
        return {};
    }

    Result IManagerDisplayService::AddToLayerStack(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
