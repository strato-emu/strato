// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IPurchaseEventManager.h"

namespace skyline::service::aocsrv {
    IPurchaseEventManager::IPurchaseEventManager(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager),
          purchasedEvent(std::make_shared<type::KEvent>(state, false)) {}

    Result IPurchaseEventManager::SetDefaultDeliveryTarget(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IPurchaseEventManager::GetPurchasedEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(purchasedEvent)};
        Logger::Debug("Purchased Event Readable Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IPurchaseEventManager::PopPurchasedProductInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
