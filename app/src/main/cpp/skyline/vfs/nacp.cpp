// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nacp.h"

namespace skyline::vfs {
    NACP::NACP(const std::shared_ptr<vfs::Backing> &backing) {
        nacpContents = backing->Read<NacpData>();

        // TODO: Select based on language settings, complete struct, yada yada

        // Iterate till we get to the first populated entry
        for (auto &entry : nacpContents.titleEntries) {
            if (entry.applicationName.front() == '\0')
                continue;

            applicationName = std::string(entry.applicationName.data(), entry.applicationName.size());
            applicationPublisher = std::string(entry.applicationPublisher.data(), entry.applicationPublisher.size());
        }
    }
}
