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
        LOGD("Library Applet Launchable Event Handle: 0x{:X}", handle);

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
        LOGD("Creating Managed Layer #{} on 'Default' Display", layerId);
        response.Push(layerId);
        return {};
    }

    Result ISelfController::SetIdleTimeDetectionExtension(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        idleTimeDetectionExtension = request.Pop<u32>();
        LOGD("Setting Idle Time Detection Extension: 0x{:X}", idleTimeDetectionExtension);
        return {};
    }

    Result ISelfController::GetIdleTimeDetectionExtension(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(idleTimeDetectionExtension);
        return {};
    }

    Result ISelfController::ReportUserIsActive(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ISelfController::IsIlluminanceAvailable(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(true);

        return {};
    }

    Result ISelfController::SetAutoSleepDisabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        autoSleepDisabled = request.Pop<u8>();
        return {};
    }

    Result ISelfController::IsAutoSleepDisabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(autoSleepDisabled);
        return {};
    }

    Result ISelfController::GetCurrentIlluminanceEx(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Values based in Ryujinx
        // https://github.com/Ryujinx/Ryujinx/blob/773e239db7ceb2c55aa15f9787add4430edcdfcf/src/Ryujinx.HLE/HOS/Services/Am/AppletAE/AllSystemAppletProxiesService/SystemAppletProxy/ISelfController.cs#L342
        response.Push<u32>(1);
        response.Push<float>(10000.0);

        return {};
    }

    Result ISelfController::GetAccumulatedSuspendedTickValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // TODO: Properly handle this after we implement game suspending
        response.Push<u64>(0);
        return {};
    }

    Result ISelfController::GetAccumulatedSuspendedTickChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(accumulatedSuspendedTickChangedEvent)};
        LOGD("Accumulated Suspended Tick Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result ISelfController::SetAlbumImageTakenNotificationEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto albumImageTakenNotificationEnabled{request.Pop<u8>()};;
        LOGD("Setting Album Image Taken Notification Enabled: {}", albumImageTakenNotificationEnabled);
        return {};
    }

    Result ISelfController::SetRecordVolumeMuted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
