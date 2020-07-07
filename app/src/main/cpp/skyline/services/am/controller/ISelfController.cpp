// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include <services/hosbinder/display.h>
#include "ISelfController.h"

namespace skyline::service::am {
    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager) : libraryAppletLaunchableEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager, Service::am_ISelfController, "am:ISelfController", {
        {0x9, SFUNC(ISelfController::GetLibraryAppletLaunchableEvent)},
        {0xB, SFUNC(ISelfController::SetOperationModeChangedNotification)},
        {0xC, SFUNC(ISelfController::SetPerformanceModeChangedNotification)},
        {0xD, SFUNC(ISelfController::SetFocusHandlingMode)},
        {0x10, SFUNC(ISelfController::SetOutOfFocusSuspendingEnabled)},
        {0x28, SFUNC(ISelfController::CreateManagedDisplayLayer)}
    }) {}

    void ISelfController::GetLibraryAppletLaunchableEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        libraryAppletLaunchableEvent->Signal();

        KHandle handle = state.process->InsertItem(libraryAppletLaunchableEvent);
        state.logger->Debug("Library Applet Launchable Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
    }

    void ISelfController::SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetOutOfFocusSuspendingEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Creating Managed Layer on Default Display");

        auto hosBinder = state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>(Service::hosbinder_IHOSBinderDriver);
        if (hosBinder->layerStatus != hosbinder::LayerStatus::Uninitialized)
            throw exception("The application is creating more than one layer");
        hosBinder->layerStatus = hosbinder::LayerStatus::Managed;

        response.Push<u64>(0);
    }
}
