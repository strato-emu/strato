// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAsyncContext.h"

namespace skyline::service::account {
    IAsyncContext::IAsyncContext(const DeviceState &state, ServiceManager &manager) : BaseService{state, manager},
        systemEvent{std::make_shared<kernel::type::KEvent>(state, true)} {}

    Result IAsyncContext::GetSystemEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(systemEvent)};
        Logger::Debug("System Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAsyncContext::Cancel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        systemEvent->Signal();
        return {};
    }

    Result IAsyncContext::HasDone(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(1);
        return {};
    }

    Result IAsyncContext::GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
