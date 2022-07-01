// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <services/am/storage/ObjIStorage.h>
#include "player_select_applet.h"

namespace skyline::applet {
    PlayerSelectApplet::PlayerSelectApplet(const DeviceState &state,
                                           service::ServiceManager &manager,
                                           std::shared_ptr<kernel::type::KEvent> onAppletStateChanged,
                                           std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet,
                                           std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet,
                                           service::applet::LibraryAppletMode appletMode)
        : IApplet{state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode} {}

    Result PlayerSelectApplet::Start() {
        // Return default user
        PushNormalDataAndSignal(std::make_shared<service::am::ObjIStorage<AccountResult>>(state, manager, AccountResult{}));

        // Notify the guest that we've finished running
        onAppletStateChanged->Signal();
        return {};
    };

    Result PlayerSelectApplet::GetResult() {
        return {};
    }

    void PlayerSelectApplet::PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) {}

    void PlayerSelectApplet::PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) {}
}
