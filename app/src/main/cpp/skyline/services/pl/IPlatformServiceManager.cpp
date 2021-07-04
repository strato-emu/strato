// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "resources/FontChineseSimplified.ttf.h"
#include "resources/FontChineseTraditional.ttf.h"
#include "resources/FontExtendedChineseSimplified.ttf.h"
#include "resources/FontKorean.ttf.h"
#include "resources/FontNintendoExtended.ttf.h"
#include "resources/FontStandard.ttf.h"
#include "IPlatformServiceManager.h"

namespace skyline::service::pl {
    struct FontEntry {
        u8 *data; //!< The font TTF data
        size_t length; //!< The length of the font TTF data
        size_t offset; //!< The offset of the font in shared memory
    };

    std::array<FontEntry, 6> fontTable{
        {
            {FontChineseSimplified, FontExtendedChineseSimplifiedLength},
            {FontChineseTraditional, FontChineseTraditionalLength},
            {FontExtendedChineseSimplified, FontExtendedChineseSimplifiedLength},
            {FontKorean, FontKoreanLength},
            {FontNintendoExtended, FontNintendoExtendedLength},
            {FontStandard, FontStandardLength}
        }
    };

    IPlatformServiceManager::IPlatformServiceManager(const DeviceState &state, ServiceManager &manager) : fontSharedMem(std::make_shared<kernel::type::KSharedMemory>(state, constant::FontSharedMemSize)), BaseService(state, manager) {
        constexpr u32 SharedFontResult{0x7F9A0218}; //!< The decrypted magic for a single font in the shared font data
        constexpr u32 SharedFontMagic{0x36F81A1E}; //!< The encrypted magic for a single font in the shared font data
        constexpr u32 SharedFontKey{SharedFontMagic ^ SharedFontResult}; //!< The XOR key for encrypting the font size

        auto ptr{reinterpret_cast<u32 *>(fontSharedMem->host.ptr)};
        for (auto &font : fontTable) {
            *ptr++ = SharedFontResult;
            *ptr++ = font.length ^ SharedFontKey;
            font.offset = reinterpret_cast<u64>(ptr) - reinterpret_cast<u64>(fontSharedMem->host.ptr);

            std::memcpy(ptr, font.data, font.length);
            ptr = reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(ptr) + font.length);
        }
    }

    Result IPlatformServiceManager::GetLoadState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        constexpr u32 FontLoaded{1}; //!< "All fonts have been loaded into memory"
        response.Push(FontLoaded);
        return {};
    }

    Result IPlatformServiceManager::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fontId{request.Pop<u32>()};
        response.Push<u32>(fontTable.at(fontId).length);
        return {};
    }

    Result IPlatformServiceManager::GetSharedMemoryAddressOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fontId{request.Pop<u32>()};
        response.Push<u32>(fontTable.at(fontId).offset);
        return {};
    }

    Result IPlatformServiceManager::GetSharedMemoryNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem<type::KSharedMemory>(fontSharedMem)};
        response.copyHandles.push_back(handle);
        return {};
    }
}
