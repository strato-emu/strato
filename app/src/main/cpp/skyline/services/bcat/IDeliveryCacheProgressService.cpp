// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include "IDeliveryCacheProgressService.h"

namespace skyline::service::bcat {
    IDeliveryCacheProgressService::IDeliveryCacheProgressService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager),
                                                                                                                      systemEvent{std::make_shared<kernel::type::KEvent>(state, true)} {}

    Result IDeliveryCacheProgressService::GetEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(systemEvent)};
        LOGD("System Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IDeliveryCacheProgressService::GetImpl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
