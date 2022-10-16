// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUserLocalCommunicationService.h"

namespace skyline::service::ldn {
    IUserLocalCommunicationService::IUserLocalCommunicationService(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager),
          event{std::make_shared<type::KEvent>(state, false)} {}

    Result IUserLocalCommunicationService::GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        static constexpr u32 StateNone{0x0};
        response.Push<u32>(StateNone);
        return {};
    }

    Result IUserLocalCommunicationService::AttachStateChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(event)};
        Logger::Debug("LDN State Change Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IUserLocalCommunicationService::InitializeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::DeviceDisabled;
    }

    Result IUserLocalCommunicationService::FinalizeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserLocalCommunicationService::InitializeSystem2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::DeviceDisabled;
    }
}
