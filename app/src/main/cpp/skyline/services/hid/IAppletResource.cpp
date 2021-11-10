// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include <kernel/types/KProcess.h>
#include "IAppletResource.h"

namespace skyline::service::hid {
    IAppletResource::IAppletResource(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAppletResource::GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem<type::KSharedMemory>(state.input->kHid)};
        Logger::Debug("HID Shared Memory Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }
}
