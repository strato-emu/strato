// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISslContext.h"

namespace skyline::service::ssl {
    ISslContext::ISslContext(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ISslContext::ImportServerPki(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        enum class CertificateFormat : u32 {
            Pem = 1,
            Der = 2,
        } certificateFormat{request.Pop<CertificateFormat>()};

        Logger::Debug("Certificate format: {}", certificateFormat);

        response.Push<u64>(0);
        return {};
    }
}
