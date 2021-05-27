// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <services/hosbinder/GraphicBufferProducer.h>
#include "ISelfController.h"

namespace skyline::service::am {
    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager) : libraryAppletLaunchableEvent(std::make_shared<type::KEvent>(state, false)), accumulatedSuspendedTickChangedEvent(std::make_shared<type::KEvent>(state, false)), BaseService(state, manager) {}

    Result ISelfController::LockExit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::UnlockExit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::GetLibraryAppletLaunchableEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        libraryAppletLaunchableEvent->Signal();

        KHandle handle{state.process->InsertItem(libraryAppletLaunchableEvent)};
        state.logger->Debug("Library Applet Launchable Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result ISelfController::SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::SetRestartMessageEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::SetOutOfFocusSuspendingEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Creating Managed Layer on Default Display");

        auto producer{hosbinder::producer.lock()};
        if (producer->layerStatus != hosbinder::LayerStatus::Uninitialized)
            throw exception("The application is creating more than one layer");
        producer->layerStatus = hosbinder::LayerStatus::Managed;

        response.Push<u64>(0);
        return {};
    }


    Result ISelfController::GetAccumulatedSuspendedTickValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // TODO: Properly handle this after we implement game suspending
        response.Push<u64>(0);
        return {};
    }

    Result ISelfController::GetAccumulatedSuspendedTickChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(accumulatedSuspendedTickChangedEvent)};
        state.logger->Debug("Accumulated Suspended Tick Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }
}
