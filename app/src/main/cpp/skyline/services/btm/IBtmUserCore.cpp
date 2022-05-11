// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IBtmUserCore.h"

namespace skyline::service::btm {
    IBtmUserCore::IBtmUserCore(const DeviceState &state, ServiceManager &manager) : bleScanEvent(std::make_shared<type::KEvent>(state, false)), bleConnectionEvent(std::make_shared<type::KEvent>(state, false)), bleServiceDiscoveryEvent(std::make_shared<type::KEvent>(state, false)), bleMtuConfigEvent(std::make_shared<type::KEvent>(state, false)), BaseService(state, manager) {}

    Result IBtmUserCore::AcquireBleScanEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(bleScanEvent)};
        response.copyHandles.push_back(handle);
        response.Push(true); // Success flag
        return {};
    }

    Result IBtmUserCore::AcquireBleConnectionEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(bleConnectionEvent)};
        response.copyHandles.push_back(handle);
        response.Push(true); // Success flag
        return {};
    }

    Result IBtmUserCore::AcquireBleServiceDiscoveryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(bleServiceDiscoveryEvent)};
        response.copyHandles.push_back(handle);
        response.Push(true); // Success flag
        return {};
    }

    Result IBtmUserCore::AcquireBleMtuConfigEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(bleMtuConfigEvent)};
        response.copyHandles.push_back(handle);
        response.Push(true); // Success flag
        return {};
    }
}
