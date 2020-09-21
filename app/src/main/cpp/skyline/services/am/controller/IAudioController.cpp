// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAudioController.h"

namespace skyline::service::am {
    IAudioController::IAudioController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAudioController::SetExpectedMasterVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        mainAppletVolume = request.Pop<float>();
        libraryAppletVolume = request.Pop<float>();
        return {};
    }

    Result IAudioController::GetMainAppletExpectedMasterVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<float>(mainAppletVolume);
        return {};
    }

    Result IAudioController::GetLibraryAppletExpectedMasterVolume(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<float>(libraryAppletVolume);
        return {};
    }
}
