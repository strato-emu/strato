// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <kernel/types/KProcess.h>
#include "IAddOnContentManager.h"
#include "IPurchaseEventManager.h"

namespace skyline::service::aocsrv {
    IAddOnContentManager::IAddOnContentManager(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager),
          addOnContentListChangedEvent(std::make_shared<type::KEvent>(state, false)) {}

    Result IAddOnContentManager::CountAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IAddOnContentManager::ListAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IAddOnContentManager::GetAddOnContentBaseId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(state.loader->nacp->nacpContents.addOnContentBaseId);
        return {};
    }

    Result IAddOnContentManager::GetAddOnContentListChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(addOnContentListChangedEvent)};
        LOGD("Add On Content List Changed Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAddOnContentManager::GetAddOnContentListChangedEventWithProcessId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(addOnContentListChangedEvent)};
        LOGD("Add On Content List Changed Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAddOnContentManager::CheckAddOnContentMountStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IAddOnContentManager::CreateEcPurchasedEventManager(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IPurchaseEventManager), session, response);
        return {};
    }
}
