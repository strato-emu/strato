// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IFile.h"

namespace skyline::service::fssrv {
    IFile::IFile(std::shared_ptr<vfs::Backing> &backing, const DeviceState &state, ServiceManager &manager) : backing(backing), BaseService(state, manager, Service::fssrv_IFile, "fssrv:IFile", {
        {0x0, SFUNC(IFile::Read)},
        {0x1, SFUNC(IFile::Write)},
        {0x3, SFUNC(IFile::SetSize)},
        {0x4, SFUNC(IFile::GetSize)}
    }) {}

    void IFile::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto readOption = request.Pop<u32>();
        request.Skip<u32>();
        auto offset = request.Pop<i64>();
        auto size = request.Pop<i64>();

        if (offset < 0) {
            state.logger->Warn("Trying to read a file with a negative offset");
            response.errorCode = constant::status::InvAddress;
            return;
        }

        if (size < 0) {
            state.logger->Warn("Trying to read a file with a negative size");
            response.errorCode = constant::status::InvSize;
            return;
        }

        response.Push<u32>(static_cast<u32>(backing->Read(state.process->GetPointer<u8>(request.outputBuf.at(0).address), offset, size)));
    }

    void IFile::Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto writeOption = request.Pop<u32>();
        request.Skip<u32>();
        auto offset = request.Pop<i64>();
        auto size = request.Pop<i64>();

        if (offset < 0) {
            state.logger->Warn("Trying to write to a file with a negative offset");
            response.errorCode = constant::status::InvAddress;
            return;
        }

        if (size < 0) {
            state.logger->Warn("Trying to write to a file with a negative size");
            response.errorCode = constant::status::InvSize;
            return;
        }

        if (request.inputBuf.at(0).size < size) {
            state.logger->Warn("The input buffer is not large enough to fit the requested size");
            response.errorCode = constant::status::InvSize;
            return;
        }

        if (backing->Write(state.process->GetPointer<u8>(request.inputBuf.at(0).address), offset, request.inputBuf.at(0).size) != size) {
            state.logger->Warn("Failed to write all data to the backing");
            response.errorCode = constant::status::GenericError;
        }
    }

    void IFile::SetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        backing->Resize(request.Pop<u64>());
    }

    void IFile::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(backing->size);
    }
}
