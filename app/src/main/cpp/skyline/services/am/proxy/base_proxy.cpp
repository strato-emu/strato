// SPDX-License-Identifier: LGPL-3.0-or-later
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
    BaseProxy::BaseProxy(const DeviceState &state, ServiceManager &manager, const Service serviceType, const std::string &serviceName, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : BaseService(state, manager, serviceType, serviceName, vTable) {}

    void BaseProxy::GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ICommonStateGetter), session, response);
    }

    void BaseProxy::GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISelfController), session, response);
    }

    void BaseProxy::GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IWindowController), session, response);
    }

    void BaseProxy::GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAudioController), session, response);
    }

    void BaseProxy::GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDisplayController), session, response);
    }

    void BaseProxy::GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ILibraryAppletCreator), session, response);
    }

    void BaseProxy::GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDebugFunctions), session, response);
    }

    void BaseProxy::GetAppletCommonFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAppletCommonFunctions), session, response);
    }
}
