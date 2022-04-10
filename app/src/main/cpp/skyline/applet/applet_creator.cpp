// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "applet_creator.h"

namespace skyline::applet {
    std::shared_ptr<service::am::IApplet> CreateApplet(
        const DeviceState &state, service::ServiceManager &manager,
        applet::AppletId appletId, const std::shared_ptr<kernel::type::KEvent> &onAppletStateChanged,
        const std::shared_ptr<kernel::type::KEvent> &onNormalDataPushFromApplet,
        const std::shared_ptr<kernel::type::KEvent> &onInteractiveDataPushFromApplet,
        service::applet::LibraryAppletMode appletMode) {
        switch (appletId) {
            default:
                throw exception("Unimplemented Applet: 0x{:X} ({})", static_cast<u32>(appletId), ToString(appletId));
        }
    }
}
