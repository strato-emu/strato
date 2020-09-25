// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "results.h"
#include "IStorage.h"

namespace skyline::service::fssrv {
    IStorage::IStorage(std::shared_ptr<vfs::Backing> &backing, const DeviceState &state, ServiceManager &manager) : backing(backing), BaseService(state, manager) {}

    Result IStorage::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset = request.Pop<i64>();
        auto size = request.Pop<i64>();

        if (offset < 0) {
            state.logger->Warn("Trying to read a file with a negative offset");
            return result::InvalidOffset;
        }

        if (size < 0) {
            state.logger->Warn("Trying to read a file with a negative size");
            return result::InvalidSize;
        }

        backing->Read(request.outputBuf.at(0).data(), offset, size);
        return {};
    }

    Result IStorage::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(backing->size);
        return {};
    }
}
