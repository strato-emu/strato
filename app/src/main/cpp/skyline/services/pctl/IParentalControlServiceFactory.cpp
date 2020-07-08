// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IParentalControlService.h"
#include "IParentalControlServiceFactory.h"

namespace skyline::service::pctl {
    IParentalControlServiceFactory::IParentalControlServiceFactory(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::pctl_IParentalControlServiceFactory, "pctl:IParentalControlServiceFactory", {
        {0x0, SFUNC(IParentalControlServiceFactory::CreateService)},
        {0x1, SFUNC(IParentalControlServiceFactory::CreateService)}

    }) {}

    void IParentalControlServiceFactory::CreateService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IParentalControlService>(state, manager), session, response);
    }
}
