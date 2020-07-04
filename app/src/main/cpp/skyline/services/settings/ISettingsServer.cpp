// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "ISettingsServer.h"

namespace skyline::service::settings {
    ISettingsServer::ISettingsServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::settings_ISettingsServer, "settings:ISettingsServer", {
        {0x1, SFUNC(ISettingsServer::GetAvailableLanguageCodes)}
    }) {}

    constexpr std::array<u64, 15> LanguageCodeList = {
        util::MakeMagic<u64>("ja"),
        util::MakeMagic<u64>("en-US"),
        util::MakeMagic<u64>("fr"),
        util::MakeMagic<u64>("de"),
        util::MakeMagic<u64>("it"),
        util::MakeMagic<u64>("es"),
        util::MakeMagic<u64>("zh-CN"),
        util::MakeMagic<u64>("ko"),
        util::MakeMagic<u64>("nl"),
        util::MakeMagic<u64>("pt"),
        util::MakeMagic<u64>("ru"),
        util::MakeMagic<u64>("zh-TW"),
        util::MakeMagic<u64>("en-GB"),
        util::MakeMagic<u64>("fr-CA"),
        util::MakeMagic<u64>("es-419")
    };

    void ISettingsServer::GetAvailableLanguageCodes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.process->WriteMemory(LanguageCodeList.data(), request.outputBuf.at(0).address, LanguageCodeList.size() * sizeof(u64));

        response.Push<i32>(LanguageCodeList.size());
    }
}
