
// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/am/storage/VectorIStorage.h>
#include <services/am/storage/TransferMemoryIStorage.h>
#include <services/am/applet/ILibraryAppletAccessor.h>
#include <kernel/types/KTransferMemory.h>
#include <kernel/types/KProcess.h>
#include "ILibraryAppletCreator.h"

namespace skyline::service::am {
    ILibraryAppletCreator::ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ILibraryAppletCreator::CreateLibraryApplet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto appletId{request.Pop<skyline::applet::AppletId>()};
        auto appletMode{request.Pop<applet::LibraryAppletMode>()};
        manager.RegisterService(SRVREG(ILibraryAppletAccessor, appletId, appletMode), session, response);
        return {};
    }

    Result ILibraryAppletCreator::CreateStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto size{request.Pop<i64>()};
        if (size < 0)
            throw exception("Cannot create an IStorage with a negative size");
        manager.RegisterService(SRVREG(VectorIStorage, size), session, response);
        return {};
    }

    Result ILibraryAppletCreator::CreateTransferMemoryStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        bool writable{request.Pop<u64>() != 0};
        i64 size{request.Pop<i64>()};
        if (size < 0)
            throw exception("Cannot create an IStorage with a negative size");
        manager.RegisterService(SRVREG(TransferMemoryIStorage, state.process->GetHandle<kernel::type::KTransferMemory>(request.copyHandles.at(0)), writable), session, response);
        return {};
    }
}
