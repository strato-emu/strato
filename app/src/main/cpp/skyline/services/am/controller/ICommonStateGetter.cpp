// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/settings.h>
#include <kernel/types/KProcess.h>
#include "ICommonStateGetter.h"

namespace skyline::service::am {
    void ICommonStateGetter::QueueMessage(ICommonStateGetter::Message message) {
        messageQueue.emplace_back(message);
        messageEvent->Signal();
    }

    ICommonStateGetter::ICommonStateGetter(const DeviceState &state, ServiceManager &manager) : messageEvent(std::make_shared<type::KEvent>(state, false)), BaseService(state, manager) {
        operationMode = static_cast<OperationMode>(state.settings->operationMode);
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
        messageQueue.pop_front();

        if (messageQueue.empty())
            messageEvent->ResetSignal();

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
            constexpr u16 HandheldResolutionW{1280};
            constexpr u16 HandheldResolutionH{720};
            response.Push<u32>(HandheldResolutionW);
            response.Push<u32>(HandheldResolutionH);
        } else if (operationMode == OperationMode::Docked) {
            constexpr u16 DockedResolutionW{1920};
            constexpr u16 DockedResolutionH{1080};
            response.Push<u32>(DockedResolutionW);
            response.Push<u32>(DockedResolutionH);
        }
        return {};
    }
}
