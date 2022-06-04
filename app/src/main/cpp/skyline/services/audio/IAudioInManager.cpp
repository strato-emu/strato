// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IAudioInManager.h"

namespace skyline::service::audio {
    IAudioInManager::IAudioInManager(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAudioInManager::ListAudioIns(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Stub to return no available audio inputs
        response.Push<u32>(0);
        return {};
    }
}
