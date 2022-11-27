// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/am/applet/IApplet.h>
#include <services/applet/common_arguments.h>
#include <jvm.h>
#include "software_keyboard_config.h"

namespace skyline::applet::swkbd {
    static_assert(sizeof(KeyboardConfigVB) == sizeof(JvmManager::KeyboardConfig));

    /**
     * @url https://switchbrew.org/wiki/Software_Keyboard
     * @brief An implementation for the Software Keyboard (swkbd) Applet which handles translating guest applet transactions to the appropriate host behavior
     */
    class SoftwareKeyboardApplet : public service::am::IApplet, service::am::EnableNormalQueue {
      private:
        /**
         * @url https://switchbrew.org/wiki/Software_Keyboard#CloseResult
         */
        enum class CloseResult : u32 {
            Enter = 0x0,
            Cancel = 0x1,
        };

        /**
         * @url https://switchbrew.org/wiki/Software_Keyboard#TextCheckResult
         */
        enum class TextCheckResult : u32 {
            Success = 0x0,
            ShowFailureDialog = 0x1,
            ShowConfirmDialog = 0x2,
        };

        static constexpr u32 SwkbdTextBytes{0x7D4}; //!< Size of the returned IStorage buffer that's used to return the input text

        static constexpr u32 MaxOneLineChars{32}; //!< The maximum number of characters for which anything other than InputFormMode::MultiLine is used

        #pragma pack(push, 1)

        /**
         * @brief The final result after the swkbd has closed
         */
        struct OutputResult {
            CloseResult closeResult;
            std::array<u8, SwkbdTextBytes> chars{};

            OutputResult(CloseResult closeResult, std::u16string_view text, bool useUtf8Storage);
        };
        static_assert(sizeof(OutputResult) == 0x7D8);

        /**
         * @brief A request for validating a string inside guest code, this is pushed via the interactive queue
         */
        struct ValidationRequest {
            u64 size;
            std::array<u8, SwkbdTextBytes> chars{};

            ValidationRequest(std::u16string_view text, bool useUtf8Storage);
        };
        static_assert(sizeof(ValidationRequest) == 0x7DC);

        /**
         * @brief The result of validating text submitted to the guest
         */
        struct ValidationResult {
            TextCheckResult result;
            std::array<char16_t, SwkbdTextBytes / sizeof(char16_t)> chars;
        };
        static_assert(sizeof(ValidationResult) == 0x7D8);

        #pragma pack(pop)

        KeyboardConfigVB config{};
        service::applet::LibraryAppletMode mode{};
        bool validationPending{};
        std::u16string currentText{};
        CloseResult currentResult{};

        jobject dialog{};

        void SendResult();

      public:
        SoftwareKeyboardApplet(const DeviceState &state, service::ServiceManager &manager, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged, std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet, std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet, service::applet::LibraryAppletMode appletMode);

        Result Start() override;

        Result GetResult() override;

        void PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) override;

        void PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) override;
    };
}
