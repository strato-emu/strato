// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

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
            u8 _pad0_[0x78];
            u64 saveDataOwnerId; //!< The ID that should be used for this application's savedata
            u8 _pad1_[0x78];
            std::array<u8, 8> seedForPseudoDeviceId; //!< Seed that is combined with the device seed for generating the pseudo-device ID
            u8 _pad2_[0xF00];
        } nacpContents{};
        static_assert(sizeof(NacpData) == 0x4000);

        NACP(const std::shared_ptr<vfs::Backing> &backing);

        std::string applicationName; //!< The name of the application in the currently selected language
        std::string applicationPublisher; //!< The publisher of the application in the currently selected language
    };
}
