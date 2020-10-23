// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IUserInterface.h"

namespace skyline::service::sm {
    IUserInterface::IUserInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IUserInterface::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IUserInterface::GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto name{request.Pop<ServiceName>()};

        if (!name)
            return result::InvalidServiceName;

        try {
            manager.NewService(name, session, response);
            return {};
        } catch (std::out_of_range &) {
            std::string_view stringName(reinterpret_cast<char *>(&name), sizeof(u64));
            state.logger->Warn("Service has not been implemented: \"{}\"", stringName);
            return result::InvalidServiceName;
        }
    }
}
