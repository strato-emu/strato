// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2020 Ryujinx Team and Contributors (https://github.com/ryujinx/)

#pragma once

#include <services/am/applet/IApplet.h>

namespace skyline::applet {
    /**
     * @brief The Web applet is utilized by the guest to display web pages using the built-in web browser
     */
    class WebApplet : public service::am::IApplet, service::am::EnableNormalQueue {
      private:
        #pragma pack(push, 1)

        /**
         * @brief Type of web-applet to launch
         * @url https://switchbrew.org/wiki/Internet_Browser#ShimKind
         */
        enum class ShimKind : u32 {
            Shop = 1,
            Login = 2,
            Offline = 3,
            Share = 4,
            Web = 5,
            Wifi = 6,
            Lobby = 7,
            Lhub = 8,
        };

        enum class WebExitReason : u32 {
            EndButtonPressed = 0,
            BackButtonPressed = 1,
            ExitRequested = 2,
            CallbackURL = 3,
            WindowClosed = 4,
            ErrorDialog = 7,
        };

        /**
         * @brief Common return value struct for all web-applet commands
         */
        struct WebCommonReturnValue {
            WebExitReason exitReason;
            u32 _pad_;
            std::array<char, 0x1000> lastUrl;
            u64 lastUrlSize;
        };
        static_assert(sizeof(WebCommonReturnValue) == 0x1010);

        /**
         * @brief The input data for the web-applet
         */
        struct WebArgHeader {
            u16 count;
            u16 _pad_;
            ShimKind shimKind;
        };

        #pragma pack(pop)


      public:
        WebApplet(const DeviceState &state, service::ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, service::applet::LibraryAppletMode appletMode);

        Result Start() override;

        Result GetResult() override;

        void PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) override;

        void PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) override;
    };
}
