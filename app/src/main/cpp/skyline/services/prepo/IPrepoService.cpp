// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IPrepoService.h"

namespace skyline::service::prepo {
    IPrepoService::IPrepoService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x2775, SFUNC(IPrepoService::SaveReportWithUser)},
    }) {}

    Result IPrepoService::SaveReportWithUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
