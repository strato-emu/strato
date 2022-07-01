// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/am/applet/IApplet.h>
#include <services/applet/common_arguments.h>
#include <input/npad_device.h>

namespace skyline::applet {
    /**
     * @brief The Controller applet is responsible for notifiying the user of a games controller requirements and for allowing user management og controllers
     */
    class ControllerApplet : public service::am::IApplet, service::am::EnableNormalQueue {
      private:
        /**
         * @brief The version of the controller applet interface that an application supports
         */
        enum class ControllerAppletVersion : u32 {
            Version3 = 0x3, // 1.0.0 - 2.3.0
            Version4 = 0x4, // 3.0.0 - 5.1.0
            Version5 = 0x5, // 6.0.0 - 7.0.1
            // No version 6
            Version7 = 0x7, // 8.0.0 - 10.2.0
            Version8 = 0x8, // 11.0.0+
        };

        /**
         * @brief The requested mode of the controller applet, determines the specific UI that should be shown to the user
         */
        enum class ControllerSupportMode : u8 {
            ShowControllerSupport = 0,
            ShowControllerStrapGuide = 1,
            ShowControllerFirmwareUpdate = 2,
            ShowControllerKeyRemappingForSystem = 3,

            MaxControllerSupportMode
        };

        /**
         * @brief The caller that is requesting the controller applet
         */
        enum class ControllerSupportCaller : u8 {
            Application = 1,
            System = 2
        };

        /**
         * @brief Common set of arguments supplied for all controller applet invocations
         */
        struct ControllerSupportArgPrivate {
            u32 argPrivateSize;
            u32 argSize;
            bool flag0;
            bool flag1;
            ControllerSupportMode mode;
            ControllerSupportCaller caller;
            input::NpadStyleSet styleSet;
            u32 joyHoldType;
        };
        static_assert(sizeof(ControllerSupportArgPrivate) == 0x14);

        /**
         * @brief Set of arguments required for the ShowControllerSupport mode, templated since the number of controller supported varies based on applet version
         */
        template<u8 NumControllersSupported>
        struct ControllerSupportArg {
            using IdentificationColor = std::array<u8, 4>; // RGBA colour code
            using ExplainText = std::array<char, 128 + 1>; // 128 chars + null terminator

            i8 playerCountMin{};
            i8 playerCountMax{};
            bool enableTakeOverConnection{};
            bool enableLeftJustify{};
            bool enablePermitJoyDual{};
            bool enableSingleMode{};
            bool enableIdentificationColor{};
            std::array<IdentificationColor, NumControllersSupported> identification_colors{};
            bool enableExplainText{};
            std::array<ExplainText, NumControllersSupported> explain_text{};
        };

        // Applet versions 3-5 inclusive allow 4 controllers maximum
        using ControllerSupportArgOld = ControllerSupportArg<4>;
        static_assert(sizeof(ControllerSupportArgOld) == 0x21C);

        // Applet versions 6-8 allow 8 controllers maximum
        using ControllerSupportArgNew = ControllerSupportArg<8>;
        static_assert(sizeof(ControllerSupportArgNew) == 0x430);

        /**
         * @brief The result type of the controller applet controller support mode
         */
        struct ControllerSupportResultInfo {
            i8 playerCount;
            u8 _pad_[3];
            input::NpadId selectedId;
            Result result;
        };
        static_assert(sizeof(ControllerSupportResultInfo) == 0xC);

        /**
         * @brief Handles the 'ShowControllerSupport' mode of the controller applet
         */
        void HandleShowControllerSupport(input::NpadStyleSet styleSet, ControllerAppletVersion version, span<u8> arg);

      public:
        ControllerApplet(const DeviceState &state, service::ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, service::applet::LibraryAppletMode appletMode);

        Result Start() override;

        Result GetResult() override;

        void PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) override;

        void PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) override;
    };
}