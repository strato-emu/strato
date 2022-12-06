// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IRequest.h"

namespace skyline::service::nifm {
    namespace result {
        constexpr Result AppletLaunchNotRequired{110, 180};
    }

    IRequest::IRequest(const DeviceState &state, ServiceManager &manager)
        : event0(std::make_shared<type::KEvent>(state, false)),
          event1(std::make_shared<type::KEvent>(state, false)),
          BaseService(state, manager) {}

    Result IRequest::GetRequestState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 Unsubmitted{1}; //!< The request has not been submitted
        response.Push<u32>(Unsubmitted);
        return {};
    }

    Result IRequest::GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IRequest::GetSystemEventReadableHandles(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(event0)};
        Logger::Debug("Request Event 0 Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);

        handle = state.process->InsertItem(event1);
        Logger::Debug("Request Event 1 Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);

        return {};
    }

    Result IRequest::Submit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IRequest::SetConnectionConfirmationOption(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IRequest::GetAppletInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::AppletLaunchNotRequired;
    }
}
