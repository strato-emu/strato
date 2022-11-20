// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2020 Ryujinx Team and Contributors (https://github.com/ryujinx/)

#pragma once

#include <services/am/applet/IApplet.h>
#include "common/language.h"

namespace skyline::applet {
    /**
     * @brief The Error Applet is utilized by the guest to display an error to the user, this class prints the supplied error to the logger
     * @url https://switchbrew.org/wiki/Error_Applet
     * @url https://switchbrew.org/wiki/Error_applet
     */
    class ErrorApplet : public service::am::IApplet, service::am::EnableNormalQueue {
      private:

        #pragma pack(push, 1)

        enum class ErrorType : u8 {
            ErrorCommonArg      = 0,
            SystemErrorArg      = 1,
            ApplicationErrorArg = 2,
            ErrorEulaArg        = 3,
            ErrorPctlArg        = 4,
            ErrorRecordArg      = 5,
            SystemUpdateEulaArg = 8
        };

        /**
         * @url https://switchbrew.org/wiki/Error_Applet#ErrorCommonHeader
         */
        struct ErrorCommonHeader {
            ErrorType type;
            u8 jump;
            u8 _pad_[0x3];
            u8 contextFlag;
            u8 resultFlag;
            u8 contextFlag2;
        };
        static_assert(sizeof(ErrorCommonHeader) == 0x8);

        /**
         * @url https://switchbrew.org/wiki/Error_Applet#ErrorCommonArg
         */
        struct ErrorCommonArg {
            ErrorCommonHeader header;
            u64 errorCode;
            Result result;
        };

        struct ApplicationErrorArg {
            ErrorCommonHeader commonHeader;
            u32 errorNumber;
            LanguageCode languageCode;
            std::array<char, 0x800> dialogMessage;
            std::array<char, 0x800> fullscreenMessage; //!< The message displayed when the user clicks on "Details", when not set this disables displaying Details
        };
        static_assert(sizeof(ApplicationErrorArg) == 0x1014);

        #pragma pack(pop)

        std::shared_ptr<service::am::IStorage> errorStorage;

        void HandleErrorCommonArg();

        void HandleApplicationErrorArg();

      public:
        ErrorApplet(const DeviceState &state, service::ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, service::applet::LibraryAppletMode appletMode);

        Result Start() override;

        Result GetResult() override;

        void PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) override;

        void PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) override;
    };
}
