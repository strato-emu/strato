// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/am/controller/ICommonStateGetter.h>
#include <services/am/controller/ISelfController.h>
#include <services/am/controller/IWindowController.h>
#include <services/am/controller/IAudioController.h>
#include <services/am/controller/IDisplayController.h>
#include <services/am/controller/ILibraryAppletCreator.h>
#include <services/am/controller/IDebugFunctions.h>
#include <services/am/controller/IAppletCommonFunctions.h>
#include "base_proxy.h"

namespace skyline::service::am {
    BaseProxy::BaseProxy(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result BaseProxy::GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ICommonStateGetter), session, response);
        return {};
    }

    Result BaseProxy::GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISelfController), session, response);
        return {};
    }

    Result BaseProxy::GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IWindowController), session, response);
        return {};
    }

    Result BaseProxy::GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAudioController), session, response);
        return {};
    }

    Result BaseProxy::GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDisplayController), session, response);
        return {};
    }

    Result BaseProxy::GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ILibraryAppletCreator), session, response);
        return {};
    }

    Result BaseProxy::GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDebugFunctions), session, response);
        return {};
    }

    Result BaseProxy::GetAppletCommonFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAppletCommonFunctions), session, response);
        return {};
    }
}
