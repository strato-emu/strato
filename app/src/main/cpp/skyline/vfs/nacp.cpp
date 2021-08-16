// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <bit>
#include "nacp.h"

namespace skyline::vfs {
    NACP::NACP(const std::shared_ptr<vfs::Backing> &backing) {
        nacpContents = backing->Read<NacpData>();

        u32 counter{0};
        for (auto &entry : nacpContents.titleEntries) {
            if (entry.applicationName.front() != '\0') {
                supportedTitleLanguages |= (1 << counter);
            }
            counter++;
        }
    }

    languages::ApplicationLanguage NACP::GetFirstSupportedTitleLanguage() {
        return static_cast<languages::ApplicationLanguage>(std::countr_zero(supportedTitleLanguages));
    }

    std::string NACP::GetApplicationName(languages::ApplicationLanguage language) {
        auto applicationName{span(nacpContents.titleEntries.at(static_cast<size_t>(language)).applicationName)};
        return std::string(applicationName.as_string(true));
    }

    std::string NACP::GetApplicationPublisher(languages::ApplicationLanguage language) {
        auto applicationPublisher{span(nacpContents.titleEntries.at(static_cast<size_t>(language)).applicationPublisher)};
        return std::string(applicationPublisher.as_string(true));
    }
}
