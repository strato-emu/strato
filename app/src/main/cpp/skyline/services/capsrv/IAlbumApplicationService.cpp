// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IAlbumApplicationService.h"

namespace skyline::service::capsrv {
    IAlbumApplicationService::IAlbumApplicationService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IAlbumApplicationService::SetShimLibraryVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto shimLibraryVersion{request.Pop<u64>()};
        auto appletResourceUserId{request.Pop<u64>()};

        return {};
    }

    Result IAlbumApplicationService::GetAlbumFileList0AafeAruidDeprecated(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 totalOutputEntries{0};

        auto pid{request.Pop<i32>()};
        auto contentType{request.Pop<ContentType>()};
        auto albumFileDateTimeStart{request.Pop<u64>()};
        auto albumFileDateTimeEnd{request.Pop<u64>()};
        auto appletResourceUserId{request.Pop<u64>()};

        response.Push<u64>(totalOutputEntries);
        return {};
    }

    Result IAlbumApplicationService::GetAlbumFileList3AaeAruid(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 totalOutputEntries{0};

        auto pid{request.Pop<i32>()};
        auto contentType{request.Pop<ContentType>()};
        auto albumFileDateTimeStart{request.Pop<u64>()};
        auto albumFileDateTimeEnd{request.Pop<u64>()};
        auto appletResourceUserId{request.Pop<u64>()};

        response.Push<u64>(totalOutputEntries);
        return {};
    }
}
