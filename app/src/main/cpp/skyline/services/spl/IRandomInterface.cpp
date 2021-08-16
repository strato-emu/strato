// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IRandomInterface.h"

#include <common.h>

namespace skyline::service::spl {
    IRandomInterface::IRandomInterface(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IRandomInterface::GetRandomBytes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto &outBuf{request.outputBuf.at(0)};

        util::FillRandomBytes(outBuf);
        return {};
    }
}
