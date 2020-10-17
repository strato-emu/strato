// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IStorage.h"
#include "IStorageAccessor.h"

namespace skyline::service::am {
    IStorageAccessor::IStorageAccessor(const DeviceState &state, ServiceManager &manager, std::shared_ptr<IStorage> parent) : parent(parent), BaseService(state, manager) {}

    Result IStorageAccessor::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<i64>(parent->content.size());
        return {};
    }

    Result IStorageAccessor::Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset{request.Pop<i64>()};
        auto size{std::min(static_cast<i64>(request.inputBuf.at(0).size()), static_cast<i64>(parent->content.size()) - offset)};

        if (offset > parent->content.size())
            return result::OutOfBounds;

        if (size)
            span(parent->content).copy_from(request.outputBuf.at(0), size);

        return {};
    }

    Result IStorageAccessor::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset{request.Pop<i64>()};
        auto size{std::min(static_cast<i64>(request.outputBuf.at(0).size()), static_cast<i64>(parent->content.size()) - offset)};

        if (offset > parent->content.size())
            return result::OutOfBounds;

        if (size > 0)
            request.outputBuf.at(0).copy_from(span(parent->content.data() + offset, size));

        return {};
    }
}
