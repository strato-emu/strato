// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IStorage.h"
#include "IStorageAccessor.h"

namespace skyline::service::am {
    IStorageAccessor::IStorageAccessor(const DeviceState &state, ServiceManager &manager, std::shared_ptr<IStorage> parent) : parent(parent), BaseService(state, manager, Service::am_IStorageAccessor, "am:IStorageAccessor", {
        {0x0, SFUNC(IStorageAccessor::GetSize)},
        {0xa, SFUNC(IStorageAccessor::Write)},
        {0xb, SFUNC(IStorageAccessor::Read)}
    }) {}

    void IStorageAccessor::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<i64>(parent->content.size());
    }

    void IStorageAccessor::Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset = request.Pop<i64>();
        auto size = request.inputBuf.at(0).size;

        if (offset + size > parent->content.size())
            throw exception("Trying to write past the end of an IStorage");

        if (offset < 0)
            throw exception("Trying to write before the start of an IStorage");

        state.process->ReadMemory(parent->content.data() + offset, request.inputBuf.at(0).address, size);
    }

    void IStorageAccessor::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset = request.Pop<u64>();
        auto size = request.outputBuf.at(0).size;

        if (offset + size > parent->content.size())
            throw exception("Trying to read past the end of an IStorage");

        state.process->WriteMemory(parent->content.data() + offset, request.outputBuf.at(0).address, size);
    }
}
