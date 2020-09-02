// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUserInterface.h"

namespace skyline::service::sm {
    IUserInterface::IUserInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x0, SFUNC(IUserInterface::Initialize)},
        {0x1, SFUNC(IUserInterface::GetService)}
    }) {}

    void IUserInterface::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void IUserInterface::GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto name = request.Pop<ServiceName>();

        if (name) {
            try {
                manager.NewService(name, session, response);
            } catch (std::out_of_range &) {
                response.errorCode = constant::status::ServiceNotReg;
                std::string_view stringName(reinterpret_cast<char *>(&name), sizeof(u64));
                state.logger->Warn("Service has not been implemented: \"{}\"", stringName);
            }
        } else {
            response.errorCode = constant::status::ServiceInvName;
        }
    }
}
