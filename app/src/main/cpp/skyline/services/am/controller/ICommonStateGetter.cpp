// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "ICommonStateGetter.h"

namespace skyline::service::am {
    void ICommonStateGetter::QueueMessage(ICommonStateGetter::Message message) {
        messageQueue.emplace(message);
        messageEvent->Signal();
    }

    ICommonStateGetter::ICommonStateGetter(const DeviceState &state, ServiceManager &manager) : messageEvent(std::make_shared<type::KEvent>(state)), BaseService(state, manager, Service::am_ICommonStateGetter, "am:ICommonStateGetter", {
        {0x0, SFUNC(ICommonStateGetter::GetEventHandle)},
        {0x1, SFUNC(ICommonStateGetter::ReceiveMessage)},
        {0x5, SFUNC(ICommonStateGetter::GetOperationMode)},
        {0x6, SFUNC(ICommonStateGetter::GetPerformanceMode)},
        {0x9, SFUNC(ICommonStateGetter::GetCurrentFocusState)},
        {0x3C, SFUNC(ICommonStateGetter::GetDefaultDisplayResolution)}
    }) {
        operationMode = static_cast<OperationMode>(state.settings->GetBool("operation_mode"));
        state.logger->Info("Switch to mode: {}", static_cast<bool>(operationMode) ? "Docked" : "Handheld");
        QueueMessage(Message::FocusStateChange);
    }

    void ICommonStateGetter::GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = state.process->InsertItem(messageEvent);
        state.logger->Debug("Applet Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
    }

    void ICommonStateGetter::ReceiveMessage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (messageQueue.empty()) {
            response.errorCode = constant::status::NoMessages;
            return;
        }

        response.Push(messageQueue.front());
        messageQueue.pop();
    }

    void ICommonStateGetter::GetCurrentFocusState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(focusState);
    }

    void ICommonStateGetter::GetOperationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(operationMode);
    }

    void ICommonStateGetter::GetPerformanceMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(static_cast<u32>(operationMode));
    }

    void ICommonStateGetter::GetDefaultDisplayResolution(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (operationMode == OperationMode::Handheld) {
            response.Push<u32>(constant::HandheldResolutionW);
            response.Push<u32>(constant::HandheldResolutionH);
        } else if (operationMode == OperationMode::Docked) {
            response.Push<u32>(constant::DockedResolutionW);
            response.Push<u32>(constant::DockedResolutionH);
        }
    }
}
