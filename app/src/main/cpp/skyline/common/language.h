// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <frozen/unordered_map.h>
#include <common.h>
#include <common/macros.h>

namespace skyline {
    using LanguageCode = u64;

    namespace constant {
        constexpr size_t OldLanguageCodeListSize{15}; //!< The size of the pre 4.0.0 language code list
        constexpr size_t NewLanguageCodeListSize{18}; //!< The size of the post 10.1.0 language code list (was 17 between 4.0.0 - 10.1.0)
    }

    namespace language {
        /**
         * @brief The list of all languages. Entries parameters are language, language code, system language index, application language index and map which holds a macro used for filtering some application languages that don't have a direct corresponding system language
         * @example #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) func(lang, code, sysIndex, appIndex, map) <br> LANGUAGES <br> #undef LANG_ENTRY
         */
        #define LANGUAGES                                               \
            LANG_ENTRY(Japanese,             ja,       0,  2, MAP)      \
            LANG_ENTRY(AmericanEnglish,      en-US,    1,  0, MAP)      \
            LANG_ENTRY(French,               fr,       2,  3, MAP)      \
            LANG_ENTRY(German,               de,       3,  4, MAP)      \
            LANG_ENTRY(Italian,              it,       4,  7, MAP)      \
            LANG_ENTRY(Spanish,              es,       5,  6, MAP)      \
            LANG_ENTRY(Chinese,              zh-CN,    6, 14, DONT_MAP) \
            LANG_ENTRY(Korean,               ko,       7, 12, MAP)      \
            LANG_ENTRY(Dutch,                nl,       8,  8, MAP)      \
            LANG_ENTRY(Portuguese,           pt,       9, 10, MAP)      \
            LANG_ENTRY(Russian,              ru,      10, 11, MAP)      \
            LANG_ENTRY(Taiwanese,            zh-TW,   11, 13, DONT_MAP) \
            LANG_ENTRY(BritishEnglish,       en-GB,   12,  1, MAP)      \
            LANG_ENTRY(CanadianFrench,       fr-CA,   13,  9, MAP)      \
            LANG_ENTRY(LatinAmericanSpanish, es-419,  14,  5, MAP)      \
            LANG_ENTRY(SimplifiedChinese,    zh-Hans, 15, 14, MAP)      \
            LANG_ENTRY(TraditionalChinese,   zh-Hant, 16, 13, MAP)      \
            LANG_ENTRY(BrazilianPortuguese,  pt-BR,   17, 10, DONT_MAP)

        /**
         * @brief Enumeration of system languages
         * @url https://switchbrew.org/wiki/Settings_services#Language
         */
        enum class SystemLanguage : u32 {
            #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) lang = sysIndex,
            LANGUAGES
            #undef LANG_ENTRY
        };

        #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) ENUM_CASE(lang);
        ENUM_STRING(SystemLanguage, LANGUAGES)
        #undef LANG_ENTRY

        /**
         * @brief Enumeration of application languages
         * @url https://switchbrew.org/wiki/NACP#ApplicationTitle
         */
        enum class ApplicationLanguage : u32 {
            #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) lang = appIndex,
            LANGUAGES
            #undef LANG_ENTRY
        };

        #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) map(lang);
        #define MAP(key) ENUM_CASE(key)
        #define DONT_MAP(key)
        ENUM_STRING(ApplicationLanguage, LANGUAGES)
        #undef LANG_ENTRY
        #undef MAP
        #undef DONT_MAP

        constexpr std::array<LanguageCode, constant::NewLanguageCodeListSize> LanguageCodeList{
            #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) util::MakeMagic<LanguageCode>(#code),
            LANGUAGES
            #undef LANG_ENTRY
        };

        constexpr LanguageCode GetLanguageCode(SystemLanguage language) {
            return LanguageCodeList.at(static_cast<size_t>(language));
        }

        /**
         * @brief Maps a system language to its corresponding application language
         */
        constexpr ApplicationLanguage GetApplicationLanguage(SystemLanguage systemLanguage) {
            #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) ENUM_CASE_PAIR(lang, ApplicationLanguage::lang);
            ENUM_SWITCH(SystemLanguage, systemLanguage, LANGUAGES, ApplicationLanguage::AmericanEnglish)
            #undef LANG_ENTRY
        }

        /**
         * @brief Maps an application language to its corresponding system language
         */
        constexpr SystemLanguage GetSystemLanguage(ApplicationLanguage applicationLanguage) {
            #define LANG_ENTRY(lang, code, sysIndex, appIndex, map) map(lang, SystemLanguage::lang);
            #define MAP(key, value) ENUM_CASE_PAIR(key, value)
            #define DONT_MAP(key, value)
            ENUM_SWITCH(ApplicationLanguage, applicationLanguage, LANGUAGES, SystemLanguage::AmericanEnglish)
            #undef LANG_ENTRY
            #undef MAP
            #undef DONT_MAP
        }

        #undef LANGUAGES
    }

    namespace region {
        /**
         * @brief The list of all regions
         * @url https://switchbrew.org/wiki/Settings_services#RegionCode_2
         */
        enum class RegionCode : i32 {
            Auto = -1, // Automatically determine region based on the selected language
            Japan = 0,
            Usa = 1,
            Europe = 2,
            Australia = 3,
            HongKongTaiwanKorea = 4,
            China = 5,
        };

        /**
         * @brief Returns the region code for the given system language
         */
        constexpr region::RegionCode GetRegionCodeForSystemLanguage(language::SystemLanguage systemLanguage) {
            switch (systemLanguage) {
                case language::SystemLanguage::Japanese:
                    return region::RegionCode::Japan;
                case language::SystemLanguage::AmericanEnglish:
                case language::SystemLanguage::CanadianFrench:
                case language::SystemLanguage::LatinAmericanSpanish:
                case language::SystemLanguage::BrazilianPortuguese:
                    return region::RegionCode::Usa;
                case language::SystemLanguage::French:
                case language::SystemLanguage::German:
                case language::SystemLanguage::Italian:
                case language::SystemLanguage::Spanish:
                case language::SystemLanguage::Dutch:
                case language::SystemLanguage::Portuguese:
                case language::SystemLanguage::Russian:
                case language::SystemLanguage::BritishEnglish:
                    return region::RegionCode::Europe;
                case language::SystemLanguage::Chinese:
                case language::SystemLanguage::SimplifiedChinese:
                case language::SystemLanguage::TraditionalChinese:
                    return region::RegionCode::China;
                case language::SystemLanguage::Taiwanese:
                case language::SystemLanguage::Korean:
                    return region::RegionCode::HongKongTaiwanKorea;
                default:
                    throw exception("Invalid system language: {}", systemLanguage);
            }
        }
    }
}
