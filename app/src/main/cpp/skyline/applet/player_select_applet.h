// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/uuid.h>
#include <services/account/IAccountServiceForApplication.h>
#include <services/am/applet/IApplet.h>

namespace skyline::applet {
    /**
     * @brief The player select applet is responsible for allowing the user to select a player to use with the application
     */
    class PlayerSelectApplet : public service::am::IApplet {
      private:
        /**
         * @brief Result structure for the player select applet
         */
        struct AccountResult {
            Result result{};
            u32 _pad_;
            service::account::UserId accountId{constant::DefaultUserId};
        };
        static_assert(sizeof(AccountResult) == 0x18);

      public:
        PlayerSelectApplet(const DeviceState &state, service::ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, service::applet::LibraryAppletMode appletMode);

        Result Start() override;

        Result GetResult() override;

        void PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) override;

        void PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) override;
    };
}