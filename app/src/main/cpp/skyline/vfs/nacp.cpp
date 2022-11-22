// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nacp.h"

namespace skyline::vfs {
    NACP::NACP(const std::shared_ptr<vfs::Backing> &backing) {
        nacpContents = backing->Read<NacpData>();

        u32 counter{};
        for (auto &entry : nacpContents.titleEntries) {
            if (entry.applicationName.front() != '\0') {
                supportedTitleLanguages |= (1 << counter);
            }
            counter++;
        }
    }

    language::ApplicationLanguage NACP::GetFirstSupportedTitleLanguage() {
        return static_cast<language::ApplicationLanguage>(std::countr_zero(supportedTitleLanguages));
    }

    language::ApplicationLanguage NACP::GetFirstSupportedLanguage() {
        return static_cast<language::ApplicationLanguage>(std::countr_zero(nacpContents.supportedLanguageFlag));
    }

    std::string NACP::GetApplicationName(language::ApplicationLanguage language) {
        auto applicationName{span(nacpContents.titleEntries.at(static_cast<size_t>(language)).applicationName)};
        return std::string(applicationName.as_string(true));
    }

    std::string NACP::GetApplicationVersion() {
        auto applicationPublisher{span(nacpContents.displayVersion)};
        return std::string(applicationPublisher.as_string(true));
    }

    std::string NACP::GetSaveDataOwnerId() {
        auto applicationTitleId{nacpContents.saveDataOwnerId};
        return fmt::format("{:016X}", applicationTitleId);
    }

    std::string NACP::GetApplicationPublisher(language::ApplicationLanguage language) {
        auto applicationPublisher{span(nacpContents.titleEntries.at(static_cast<size_t>(language)).applicationPublisher)};
        return std::string(applicationPublisher.as_string(true));
    }
}
