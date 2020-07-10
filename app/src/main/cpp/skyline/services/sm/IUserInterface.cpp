// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUserInterface.h"

namespace skyline::service::sm {
    IUserInterface::IUserInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::sm_IUserInterface, "sm:IUserInterface", {
        {0x0, SFUNC(IUserInterface::Initialize)},
        {0x1, SFUNC(IUserInterface::GetService)}
    }) {}

    void IUserInterface::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void IUserInterface::GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string serviceName(request.PopString());
        serviceName.resize(std::min(8UL, serviceName.size()));

        if (serviceName.empty()) {
            response.errorCode = constant::status::ServiceInvName;
        } else {
            try {
                manager.NewService(serviceName, session, response);
            } catch (std::out_of_range &) {
                response.errorCode = constant::status::ServiceNotReg;
                state.logger->Warn("Service has not been implemented: \"{}\"", serviceName);
            }
        }
    }
}
