// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include <services/hosbinder/display.h>
#include "ISelfController.h"

namespace skyline::service::am {
    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager) : libraryAppletLaunchableEvent(std::make_shared<type::KEvent>(state)), accumulatedSuspendedTickChangedEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager, Service::am_ISelfController, "am:ISelfController", {
        {0x1, SFUNC(ISelfController::LockExit)},
        {0x2, SFUNC(ISelfController::UnlockExit)},
        {0x9, SFUNC(ISelfController::GetLibraryAppletLaunchableEvent)},
        {0xB, SFUNC(ISelfController::SetOperationModeChangedNotification)},
        {0xC, SFUNC(ISelfController::SetPerformanceModeChangedNotification)},
        {0xD, SFUNC(ISelfController::SetFocusHandlingMode)},
        {0xE, SFUNC(ISelfController::SetRestartMessageEnabled)},
        {0x10, SFUNC(ISelfController::SetOutOfFocusSuspendingEnabled)},
        {0x28, SFUNC(ISelfController::CreateManagedDisplayLayer)},
        {0x5B, SFUNC(ISelfController::GetLibraryAppletLaunchableEvent)}
    }) {}

    void ISelfController::LockExit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::UnlockExit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::GetLibraryAppletLaunchableEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        libraryAppletLaunchableEvent->Signal();

        KHandle handle = state.process->InsertItem(libraryAppletLaunchableEvent);
        state.logger->Debug("Library Applet Launchable Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
    }

    void ISelfController::SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetRestartMessageEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetOutOfFocusSuspendingEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Creating Managed Layer on Default Display");

        auto hosBinder = state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>(Service::hosbinder_IHOSBinderDriver);
        if (hosBinder->layerStatus != hosbinder::LayerStatus::Uninitialized)
            throw exception("The application is creating more than one layer");
        hosBinder->layerStatus = hosbinder::LayerStatus::Managed;

        response.Push<u64>(0);
    }

    void ISelfController::GetAccumulatedSuspendedTickChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = state.process->InsertItem(accumulatedSuspendedTickChangedEvent);
        state.logger->Debug("Accumulated Suspended Tick Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
    }
}
