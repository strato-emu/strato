// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/settings.h>
#include <kernel/types/KProcess.h>
#include "ICommonStateGetter.h"

namespace skyline::service::am {
    void ICommonStateGetter::QueueMessage(ICommonStateGetter::Message message) {
        messageQueue.emplace(message);
        messageEvent->Signal();
    }

    ICommonStateGetter::ICommonStateGetter(const DeviceState &state, ServiceManager &manager) : messageEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager) {
        operationMode = static_cast<OperationMode>(state.settings->GetBool("operation_mode"));
        state.logger->Info("Switch to mode: {}", static_cast<bool>(operationMode) ? "Docked" : "Handheld");
        QueueMessage(Message::FocusStateChange);
    }

    Result ICommonStateGetter::GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(messageEvent)};
        state.logger->Debug("Applet Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result ICommonStateGetter::ReceiveMessage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (messageQueue.empty())
            return result::NoMessages;

        response.Push(messageQueue.front());
        messageQueue.pop();
        return {};
    }

    Result ICommonStateGetter::GetCurrentFocusState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(focusState);
        return {};
    }

    Result ICommonStateGetter::GetOperationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(operationMode);
        return {};
    }

    Result ICommonStateGetter::GetPerformanceMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(operationMode));
        return {};
    }

    Result ICommonStateGetter::GetDefaultDisplayResolution(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (operationMode == OperationMode::Handheld) {
            response.Push<u32>(constant::HandheldResolutionW);
            response.Push<u32>(constant::HandheldResolutionH);
        } else if (operationMode == OperationMode::Docked) {
            response.Push<u32>(constant::DockedResolutionW);
            response.Push<u32>(constant::DockedResolutionH);
        }
        return {};
    }
}
