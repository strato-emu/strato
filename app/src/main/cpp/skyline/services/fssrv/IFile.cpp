// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "results.h"
#include "IFile.h"

namespace skyline::service::fssrv {
    IFile::IFile(std::shared_ptr<vfs::Backing> &backing, const DeviceState &state, ServiceManager &manager) : backing(backing), BaseService(state, manager) {}

    Result IFile::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto readOption = request.Pop<u32>();
        request.Skip<u32>();
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

        response.Push<u32>(static_cast<u32>(backing->Read(request.outputBuf.at(0).data(), offset, size)));
        return {};
    }

    Result IFile::Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto writeOption = request.Pop<u32>();
        request.Skip<u32>();
        auto offset = request.Pop<i64>();
        auto size = request.Pop<i64>();

        if (offset < 0) {
            state.logger->Warn("Trying to write to a file with a negative offset");
            return result::InvalidOffset;
        }

        if (size < 0) {
            state.logger->Warn("Trying to write to a file with a negative size");
            return result::InvalidSize;
        }

        if (request.inputBuf.at(0).size() < size) {
            state.logger->Warn("The input buffer is not large enough to fit the requested size");
            return result::InvalidSize;
        }

        if (backing->Write(request.inputBuf.at(0).data(), offset, request.inputBuf.at(0).size()) != size) {
            state.logger->Warn("Failed to write all data to the backing");
            return result::UnexpectedFailure;
        }

        return {};
    }

    Result IFile::Flush(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IFile::SetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        backing->Resize(request.Pop<u64>());
        return {};
    }

    Result IFile::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(backing->size);
        return {};
    }
}
