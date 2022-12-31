// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <kernel/types/KProcess.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include "ISelfController.h"

namespace skyline::service::am {
    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager)
        : libraryAppletLaunchableEvent(std::make_shared<type::KEvent>(state, false)),
          accumulatedSuspendedTickChangedEvent(std::make_shared<type::KEvent>(state, true)),
          hosbinder(manager.CreateOrGetService<hosbinder::IHOSBinderDriver>("dispdrv")),
          BaseService(state, manager) {}

    Result ISelfController::Exit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        throw nce::NCE::ExitException(true);
    }

    Result ISelfController::LockExit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::UnlockExit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::GetLibraryAppletLaunchableEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        libraryAppletLaunchableEvent->Signal();

        KHandle handle{state.process->InsertItem(libraryAppletLaunchableEvent)};
        Logger::Debug("Library Applet Launchable Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result ISelfController::SetScreenShotPermission(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
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

    Result ISelfController::SetAlbumImageOrientation(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto layerId{hosbinder->CreateLayer(hosbinder::DisplayId::Default)};
        Logger::Debug("Creating Managed Layer #{} on 'Default' Display", layerId);
        response.Push(layerId);
        return {};
    }

    Result ISelfController::SetIdleTimeDetectionExtension(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        idleTimeDetectionExtension = request.Pop<u32>();
        Logger::Debug("Setting Idle Time Detection Extension: 0x{:X}", idleTimeDetectionExtension);
        return {};
    }

    Result ISelfController::GetIdleTimeDetectionExtension(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(idleTimeDetectionExtension);
        return {};
    }

    Result ISelfController::GetAccumulatedSuspendedTickValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // TODO: Properly handle this after we implement game suspending
        response.Push<u64>(0);
        return {};
    }

    Result ISelfController::GetAccumulatedSuspendedTickChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(accumulatedSuspendedTickChangedEvent)};
        Logger::Debug("Accumulated Suspended Tick Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result ISelfController::SetAlbumImageTakenNotificationEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto albumImageTakenNotificationEnabled{request.Pop<u8>()};;
        Logger::Debug("Setting Album Image Taken Notification Enabled: {}", albumImageTakenNotificationEnabled);
        return {};
    }

    Result ISelfController::SetRecordVolumeMuted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
