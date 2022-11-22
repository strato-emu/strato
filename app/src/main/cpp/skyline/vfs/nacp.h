// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/language.h>
#include "backing.h"

namespace skyline::vfs {
    /**
     * @brief The NACP class provides easy access to the data found in an NACP file
     * @url https://switchbrew.org/wiki/NACP_Format
     */
    class NACP {
      private:
        /**
         * @brief The title data of an application for one language
         */
        struct ApplicationTitle {
            std::array<char, 0x200> applicationName; //!< The name of the application
            std::array<char, 0x100> applicationPublisher; //!< The publisher of the application
        };
        static_assert(sizeof(ApplicationTitle) == 0x300);

      public:
        struct NacpData {
            std::array<ApplicationTitle, 0x10> titleEntries; //!< Title entries for each language
            u8 _pad0_[0x2C];
            u32 supportedLanguageFlag; //!< A bitmask containing the game's supported languages
            u8 _pad1_[0x30];
            std::array<char, 0x10> displayVersion; //!< The user-readable version of the application
            u8 _pad4_[0x8];
            u64 saveDataOwnerId; //!< The ID that should be used for this application's savedata
            u8 _pad2_[0x78];
            std::array<u8, 8> seedForPseudoDeviceId; //!< Seed that is combined with the device seed for generating the pseudo-device ID
            u8 _pad3_[0xF00];
        } nacpContents{};
        static_assert(sizeof(NacpData) == 0x4000);

        u32 supportedTitleLanguages{}; //!< A bitmask containing the available title entry languages and game icons

        NACP(const std::shared_ptr<vfs::Backing> &backing);

        language::ApplicationLanguage GetFirstSupportedTitleLanguage();

        language::ApplicationLanguage GetFirstSupportedLanguage();

        std::string GetApplicationName(language::ApplicationLanguage language);

        std::string GetApplicationVersion();

        std::string GetSaveDataOwnerId();

        std::string GetApplicationPublisher(language::ApplicationLanguage language);
    };
}
