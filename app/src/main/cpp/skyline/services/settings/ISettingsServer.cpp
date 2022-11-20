// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common/language.h>
#include "ISettingsServer.h"
#include <common/settings.h>

namespace skyline::service::settings {
    ISettingsServer::ISettingsServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ISettingsServer::GetLanguageCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto systemLanguage{language::GetApplicationLanguage(*state.settings->systemLanguage)};

        response.Push(language::GetLanguageCode(language::GetSystemLanguage(systemLanguage)));
        return {};
    }

    Result ISettingsServer::GetAvailableLanguageCodes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.outputBuf.at(0).copy_from(span(language::LanguageCodeList).first(constant::OldLanguageCodeListSize));
        response.Push<i32>(constant::OldLanguageCodeListSize);
        return {};
    }

    Result ISettingsServer::MakeLanguageCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(language::LanguageCodeList.at(static_cast<size_t>(request.Pop<i32>())));
        return {};
    }

    Result ISettingsServer::GetAvailableLanguageCodeCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<i32>(constant::OldLanguageCodeListSize);
        return {};
    }

    Result ISettingsServer::GetAvailableLanguageCodes2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.outputBuf.at(0).copy_from(language::LanguageCodeList);
        response.Push<i32>(constant::NewLanguageCodeListSize);
        return {};
    }

    Result ISettingsServer::GetAvailableLanguageCodeCount2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<i32>(constant::NewLanguageCodeListSize);
        return {};
    }

    Result ISettingsServer::GetRegionCode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        region::RegionCode regionCode{*state.settings->systemRegion};

        if (regionCode == region::RegionCode::Auto)
            regionCode = region::GetRegionCodeForSystemLanguage(*state.settings->systemLanguage);

        response.Push(regionCode);
        return {};
    }
}
