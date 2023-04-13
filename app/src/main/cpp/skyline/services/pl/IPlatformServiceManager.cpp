// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IPlatformServiceManager.h"
#include "shared_font_core.h"

namespace skyline::service::pl {
    IPlatformServiceManager::IPlatformServiceManager(const DeviceState &state, ServiceManager &manager, SharedFontCore &core) : BaseService(state, manager), core(core) {}

    Result IPlatformServiceManager::RequestLoad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        Logger::Debug("Requested a shared font to be loaded: {}", request.Pop<u32>());
        return {};
    }

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

    Result IPlatformServiceManager::GetSharedFontInOrderOfPriority(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto languageCode{request.Pop<u64>()};

        std::vector<u32> fontCodes{};
        std::vector<u32> fontOffsets{};
        std::vector<u32> fontSizes{};

        // TODO: actual font priority
        for (u32 i{}; i < core.fonts.size(); i++) {
            fontCodes.push_back(i);
            auto region{core.fonts.at(i)};
            fontOffsets.push_back(region.offset);
            fontSizes.push_back(region.length);
        }

        std::memset(request.outputBuf.at(0).data(), 0, request.outputBuf.at(0).size());
        std::memset(request.outputBuf.at(1).data(), 0, request.outputBuf.at(1).size());
        std::memset(request.outputBuf.at(2).data(), 0, request.outputBuf.at(2).size());

        request.outputBuf.at(0).copy_from(fontCodes);
        request.outputBuf.at(1).copy_from(fontOffsets);
        request.outputBuf.at(2).copy_from(fontSizes);

        response.Push(static_cast<u32>(core.fonts.size()));
        response.Push(static_cast<u32>(fontCodes.size()));

        return {};
    }
}
