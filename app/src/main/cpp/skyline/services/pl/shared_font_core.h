// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <os.h>
#include <kernel/types/KProcess.h>
#include <vfs/os_filesystem.h>

namespace skyline::service::pl {
    /**
     * @brief A persistent object for managing the shared memory utilized by shared fonts
     */
    struct SharedFontCore {
        static constexpr u32 FontSharedMemSize{0x140A000}; //!< The total size of the font shared memory
        std::shared_ptr<kernel::type::KSharedMemory> sharedFontMemory; //!< The KSharedMemory that stores the TTF data of all shared fonts

        struct FontEntry {
            std::string path; //!< The path of the font asset
            u32 length; //!< The length of the font TTF data
            u32 offset; //!< The offset of the font in shared memory
        };

        std::array<FontEntry, 6> fonts{
            {
                {"FontStandard.ttf"},
                {"FontChineseSimplified.ttf"},
                {"FontExtendedChineseSimplified.ttf"},
                {"FontChineseTraditional.ttf"},
                {"FontKorean.ttf"},
                {"FontNintendoExtended.ttf"},
            }
        };

        SharedFontCore(const DeviceState &state) : sharedFontMemory(std::make_shared<kernel::type::KSharedMemory>(state, FontSharedMemSize)) {
            constexpr u32 SharedFontResult{0x7F9A0218}; //!< The decrypted magic for a single font in the shared font data
            constexpr u32 SharedFontMagic{0x36F81A1E}; //!< The encrypted magic for a single font in the shared font data
            constexpr u32 SharedFontKey{SharedFontMagic ^ SharedFontResult}; //!< The XOR key for encrypting the font size

            auto fontsDirectory{std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "fonts/")};
            auto ptr{reinterpret_cast<u32 *>(sharedFontMemory->host.data())};
            for (auto &font : fonts) {
                std::shared_ptr<vfs::Backing> fontFile;
                if (fontsDirectory->FileExists(font.path))
                    fontFile = fontsDirectory->OpenFile(font.path);
                else
                    fontFile = state.os->assetFileSystem->OpenFile("fonts/" + font.path);

                font.length = static_cast<u32>(fontFile->size);

                *ptr++ = util::SwapEndianness(SharedFontResult);
                *ptr++ = util::SwapEndianness(font.length ^ SharedFontKey);
                font.offset = static_cast<u32>(reinterpret_cast<uintptr_t>(ptr) - reinterpret_cast<uintptr_t>(sharedFontMemory->host.data()));
                fontFile->Read(span<u8>(reinterpret_cast<u8 *>(ptr), font.length));
                ptr = reinterpret_cast<u32 *>(reinterpret_cast<u8 *>(ptr) + font.length);
            }
        }
    };
}
