// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include "ILblController.h"

namespace skyline::service::lbl {
    ILblController::ILblController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ILblController::SetBrightnessReflectionDelayLevel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ILblController::GetBrightnessReflectionDelayLevel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<float>(0.0f);
        return {};
    }

    Result ILblController::SetCurrentAmbientLightSensorMapping(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ILblController::GetCurrentAmbientLightSensorMapping(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result ILblController::SetCurrentBrightnessSettingForVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto brightnessSettingForVrMode{request.Pop<float>()};
        if(!std::isfinite(brightnessSettingForVrMode))
            brightnessSettingForVrMode = 0.0f;

        currentBrightnessSettingForVrMode = brightnessSettingForVrMode;

        return {};
    }

    Result ILblController::GetCurrentBrightnessSettingForVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto brightnessSettingForVrMode{currentBrightnessSettingForVrMode};
        if (!std::isfinite(brightnessSettingForVrMode))
            brightnessSettingForVrMode = 0.0f;

        response.Push<float>(brightnessSettingForVrMode);

        return {};
    }

    Result ILblController::EnableVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        vrModeEnabled = true;
        return {};
    }

    Result ILblController::DisableVrMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        vrModeEnabled = false;
        return {};
    }

    Result ILblController::IsVrModeEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(vrModeEnabled);
        return {};
    }
}
