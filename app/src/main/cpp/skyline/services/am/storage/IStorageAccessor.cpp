// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IStorage.h"
#include "IStorageAccessor.h"

namespace skyline::service::am {
    IStorageAccessor::IStorageAccessor(const DeviceState &state, ServiceManager &manager, std::shared_ptr<IStorage> parent) : parent(std::move(parent)), BaseService(state, manager) {}

    Result IStorageAccessor::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<i64>(static_cast<i64>(parent->GetSpan().size()));
        return {};
    }

    Result IStorageAccessor::Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset{request.Pop<i64>()};

        if (!parent->writable)
            return result::ObjectInvalid;
        auto storageSpan{parent->GetSpan()};
        if (offset < 0 || offset > storageSpan.size())
            return result::OutOfBounds;

        size_t size{std::min(request.inputBuf.at(0).size(), storageSpan.size() - static_cast<size_t>(offset))};

        if (size)
            storageSpan.subspan(static_cast<size_t>(offset)).copy_from(request.inputBuf.at(0), size);

        return {};
    }

    Result IStorageAccessor::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset{request.Pop<i64>()};

        auto storageSpan{parent->GetSpan()};
        if (offset < 0 || offset > storageSpan.size())
            return result::OutOfBounds;

        size_t size{std::min(request.outputBuf.at(0).size(), storageSpan.size() - static_cast<size_t>(offset))};

        if (size)
            request.outputBuf.at(0).copy_from(span(storageSpan.data() + offset, size));

        return {};
    }
}
