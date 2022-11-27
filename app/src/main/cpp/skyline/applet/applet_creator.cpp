// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "applet_creator.h"
#include "controller_applet.h"
#include "error_applet.h"
#include "player_select_applet.h"
#include "web_applet.h"
#include "swkbd/software_keyboard_applet.h"

namespace skyline::applet {
    std::shared_ptr<service::am::IApplet> CreateApplet(
        const DeviceState &state, service::ServiceManager &manager,
        applet::AppletId appletId, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged,
        std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet,
        std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet,
        service::applet::LibraryAppletMode appletMode) {
        switch (appletId) {
            case AppletId::LibraryAppletController:
                return std::make_shared<ControllerApplet>(state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode);
            case AppletId::LibraryAppletPlayerSelect:
                return std::make_shared<PlayerSelectApplet>(state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode);
            case AppletId::LibraryAppletSwkbd:
                return std::make_shared<swkbd::SoftwareKeyboardApplet>(state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode);
            case AppletId::LibraryAppletError:
                return std::make_shared<ErrorApplet>(state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode);
            case AppletId::LibraryAppletOfflineWeb:
            case AppletId::LibraryAppletShop:
                return std::make_shared<WebApplet>(state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode);
            default:
                throw exception{"Unimplemented Applet: 0x{:X} ({})", static_cast<u32>(appletId), ToString(appletId)};
        }
    }
}
