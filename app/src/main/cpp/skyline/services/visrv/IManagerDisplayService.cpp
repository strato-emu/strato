// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <services/hosbinder/GraphicBufferProducer.h>
#include <services/hosbinder/display.h>
#include "IManagerDisplayService.h"

namespace skyline::service::visrv {
    IManagerDisplayService::IManagerDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, {
        {0x7DA, SFUNC(IManagerDisplayService::CreateManagedLayer)},
        {0x7DB, SFUNC(IManagerDisplayService::DestroyManagedLayer)},
        {0x7DC, SFUNC(IDisplayService::CreateStrayLayer)},
        {0x1770, SFUNC(IManagerDisplayService::AddToLayerStack)}
    }) {}

    Result IManagerDisplayService::CreateManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u32>();
        auto displayId = request.Pop<u64>();
        state.logger->Debug("Creating Managed Layer on Display: {}", displayId);

        auto producer = hosbinder::producer.lock();
        if (producer->layerStatus != hosbinder::LayerStatus::Uninitialized)
            throw exception("The application is creating more than one layer");
        producer->layerStatus = hosbinder::LayerStatus::Managed;

        response.Push<u64>(0); // There's only one layer
        return {};
    }

    Result IManagerDisplayService::DestroyManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId = request.Pop<u64>();
        state.logger->Debug("Destroying Managed Layer: {}", layerId);

        auto producer = hosbinder::producer.lock();
        if (producer->layerStatus == hosbinder::LayerStatus::Uninitialized)
            state.logger->Warn("The application is destroying an uninitialized layer");
        producer->layerStatus = hosbinder::LayerStatus::Uninitialized;

        return {};
    }

    Result IManagerDisplayService::AddToLayerStack(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
