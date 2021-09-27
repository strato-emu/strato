// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/language.h>
#include "ISettingsServer.h"

namespace skyline::service::settings {
    ISettingsServer::ISettingsServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ISettingsServer::GetAvailableLanguageCodes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.outputBuf.at(0).copy_from(span(language::LanguageCodeList).first(constant::OldLanguageCodeListSize));
        response.Push<i32>(constant::OldLanguageCodeListSize);
        return {};
    }

    Result ISettingsServer::MakeLanguageCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(language::LanguageCodeList.at(request.Pop<i32>()));
        return {};
    }

    Result ISettingsServer::GetAvailableLanguageCodes2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.outputBuf.at(0).copy_from(language::LanguageCodeList);
        response.Push<i32>(constant::NewLanguageCodeListSize);
        return {};
    }
}
