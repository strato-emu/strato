// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    using LanguageCode = u64;

    namespace constant {
        constexpr size_t OldLanguageCodeListSize{15}; //!< The size of the pre 4.0.0 language code list
        constexpr size_t NewLanguageCodeListSize{17}; //!< The size of the post 4.0.0 language code list
    }

    namespace languages {
        constexpr std::array<LanguageCode, constant::NewLanguageCodeListSize> LanguageCodeList{
            util::MakeMagic<LanguageCode>("ja"),
            util::MakeMagic<LanguageCode>("en-US"),
            util::MakeMagic<LanguageCode>("fr"),
            util::MakeMagic<LanguageCode>("de"),
            util::MakeMagic<LanguageCode>("it"),
            util::MakeMagic<LanguageCode>("es"),
            util::MakeMagic<LanguageCode>("zh-CN"),
            util::MakeMagic<LanguageCode>("ko"),
            util::MakeMagic<LanguageCode>("nl"),
            util::MakeMagic<LanguageCode>("pt"),
            util::MakeMagic<LanguageCode>("ru"),
            util::MakeMagic<LanguageCode>("zh-TW"),
            util::MakeMagic<LanguageCode>("en-GB"),
            util::MakeMagic<LanguageCode>("fr-CA"),
            util::MakeMagic<LanguageCode>("es-419"),
            util::MakeMagic<LanguageCode>("zh-Hans"),
            util::MakeMagic<LanguageCode>("zh-Hant"),
        };

        constexpr LanguageCode GetLanguageCode(SystemLanguage language) {
            return LanguageCodeList.at(static_cast<size_t>(language));
        }
    }
}