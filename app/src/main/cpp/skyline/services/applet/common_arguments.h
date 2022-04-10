// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

namespace skyline::service::applet {
    /**
     * @brief Specifies how the applet should run
     * @url https://switchbrew.org/wiki/Applet_Manager_services#LibraryAppletMode
     */
    enum class LibraryAppletMode : u32 {
        AllForeground = 0x0,
        PartialForeground = 0x1,
        NoUi = 0x2,
        PartialForegroundWithIndirectDisplay = 0x3,
        AllForegroundInitiallyHidden = 0x4,
    };
    /**
    * @brief Common arguments to all LibraryApplets
    * @url https://switchbrew.org/wiki/Applet_Manager_services#CommonArguments
    */
    struct CommonArguments {
        u32 version;
        u32 size;
        u32 apiVersion;
        u32 themeColor;
        u64 playStartupSound;
        u64 systemTick;
    };
}
