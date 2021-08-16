// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <frozen/unordered_map.h>
#include <common.h>

namespace skyline {
    using LanguageCode = u64;

    namespace constant {
        constexpr size_t OldLanguageCodeListSize{15}; //!< The size of the pre 4.0.0 language code list
        constexpr size_t NewLanguageCodeListSize{18}; //!< The size of the post 10.1.0 language code list (was 17 between 4.0.0 - 10.1.0)
    }

    namespace languages {
        enum class SystemLanguage : u32 {
            Japanese = 0,
            AmericanEnglish,
            French,
            German,
            Italian,
            Spanish,
            Chinese,
            Korean,
            Dutch,
            Portuguese,
            Russian,
            Taiwanese,
            BritishEnglish,
            CanadianFrench,
            LatinAmericanSpanish,
            SimplifiedChinese,
            TraditionalChinese,
            BrazilianPortuguese,
        };

        enum class ApplicationLanguage : u32 {
            AmericanEnglish = 0,
            BritishEnglish,
            Japanese,
            French,
            German,
            LatinAmericanSpanish,
            Spanish,
            Italian,
            Dutch,
            CanadianFrench,
            Portuguese,
            Russian,
            Korean,
            TraditionalChinese,
            SimplifiedChinese,
        };

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
            util::MakeMagic<LanguageCode>("pt-BR"),
        };

        constexpr LanguageCode GetLanguageCode(SystemLanguage language) {
            return LanguageCodeList.at(static_cast<size_t>(language));
        }

        #define LANG_MAP_ENTRY(lang) {SystemLanguage::lang, ApplicationLanguage::lang}
        #define LANG_MAP_ENTRY_CST(sysLang, appLang) {SystemLanguage::sysLang, ApplicationLanguage::appLang}
        constexpr frz::unordered_map<SystemLanguage, ApplicationLanguage, constant::NewLanguageCodeListSize> SystemToAppMap{
            LANG_MAP_ENTRY(Japanese),
            LANG_MAP_ENTRY(AmericanEnglish),
            LANG_MAP_ENTRY(French),
            LANG_MAP_ENTRY(German),
            LANG_MAP_ENTRY(Italian),
            LANG_MAP_ENTRY(Spanish),
            LANG_MAP_ENTRY_CST(Chinese, SimplifiedChinese),
            LANG_MAP_ENTRY(Korean),
            LANG_MAP_ENTRY(Dutch),
            LANG_MAP_ENTRY(Portuguese),
            LANG_MAP_ENTRY(Russian),
            LANG_MAP_ENTRY_CST(Taiwanese, TraditionalChinese),
            LANG_MAP_ENTRY(BritishEnglish),
            LANG_MAP_ENTRY(CanadianFrench),
            LANG_MAP_ENTRY(LatinAmericanSpanish),
            LANG_MAP_ENTRY(SimplifiedChinese),
            LANG_MAP_ENTRY(TraditionalChinese),
            LANG_MAP_ENTRY_CST(BrazilianPortuguese, Portuguese),
        };
        #undef LANG_MAP_ENTRY
        #undef LANG_MAP_ENTRY_CST

        constexpr ApplicationLanguage GetApplicationLanguage(SystemLanguage systemLanguage) {
            return SystemToAppMap.at(systemLanguage);
        }

        #define LANG_MAP_ENTRY_REVERSE(lang) {ApplicationLanguage::lang, SystemLanguage::lang}
        constexpr frz::unordered_map<ApplicationLanguage, SystemLanguage, constant::OldLanguageCodeListSize> AppToSystemMap{
            LANG_MAP_ENTRY_REVERSE(Japanese),
            LANG_MAP_ENTRY_REVERSE(AmericanEnglish),
            LANG_MAP_ENTRY_REVERSE(French),
            LANG_MAP_ENTRY_REVERSE(German),
            LANG_MAP_ENTRY_REVERSE(Italian),
            LANG_MAP_ENTRY_REVERSE(Spanish),
            LANG_MAP_ENTRY_REVERSE(Korean),
            LANG_MAP_ENTRY_REVERSE(Dutch),
            LANG_MAP_ENTRY_REVERSE(Portuguese),
            LANG_MAP_ENTRY_REVERSE(Russian),
            LANG_MAP_ENTRY_REVERSE(BritishEnglish),
            LANG_MAP_ENTRY_REVERSE(CanadianFrench),
            LANG_MAP_ENTRY_REVERSE(LatinAmericanSpanish),
            LANG_MAP_ENTRY_REVERSE(SimplifiedChinese),
            LANG_MAP_ENTRY_REVERSE(TraditionalChinese),
        };
        #undef LANG_MAP_ENTRY_REVERSE

        constexpr SystemLanguage GetSystemLanguage(ApplicationLanguage applicationLanguage) {
            return AppToSystemMap.at(applicationLanguage);
        }
    }
}