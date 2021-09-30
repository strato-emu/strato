// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IPlatformServiceManager.h"
#include "shared_font_core.h"

namespace skyline::service::pl {
    IPlatformServiceManager::IPlatformServiceManager(const DeviceState &state, ServiceManager &manager, SharedFontCore &core) : BaseService(state, manager), core(core) {}

    Result IPlatformServiceManager::GetLoadState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 FontLoaded{1}; //!< "All fonts have been loaded into memory"
        response.Push(FontLoaded);
        return {};
    }

    Result IPlatformServiceManager::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fontId{request.Pop<u32>()};
        response.Push<u32>(core.fonts.at(fontId).length);
        return {};
    }

    Result IPlatformServiceManager::GetSharedMemoryAddressOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fontId{request.Pop<u32>()};
        response.Push<u32>(core.fonts.at(fontId).offset);
        return {};
    }

    Result IPlatformServiceManager::GetSharedMemoryNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem<type::KSharedMemory>(core.sharedFontMemory)};
        response.copyHandles.push_back(handle);
        return {};
    }
}
