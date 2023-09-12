// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ICaptureControllerService.h"

namespace skyline::service::capsrv {
    ICaptureControllerService::ICaptureControllerService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ICaptureControllerService::SetShimLibraryVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto shimLibraryVersion{request.Pop<u64>()};
        auto appletResourceUserId{request.Pop<u64>()};

        return {};
    }
}
