// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/am/storage/IStorage.h>
#include <services/am/applet/ILibraryAppletAccessor.h>
#include "ILibraryAppletCreator.h"

namespace skyline::service::am {
    ILibraryAppletCreator::ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x0, SFUNC(ILibraryAppletCreator::CreateLibraryApplet)},
        {0xA, SFUNC(ILibraryAppletCreator::CreateStorage)}
    }) {}

    void ILibraryAppletCreator::CreateLibraryApplet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ILibraryAppletAccessor), session, response);
    }

    void ILibraryAppletCreator::CreateStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto size = request.Pop<i64>();

        if (size < 0)
            throw exception("Cannot create an IStorage with a negative size");

        manager.RegisterService(std::make_shared<IStorage>(state, manager, size), session, response);
    }
}
