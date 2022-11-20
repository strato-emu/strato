// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "error_applet.h"
#include "services/am/storage/ObjIStorage.h"

namespace skyline::applet {
    ErrorApplet::ErrorApplet(const DeviceState &state,
                             service::ServiceManager &manager,
                             std::shared_ptr<kernel::type::KEvent> onAppletStateChanged,
                             std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet,
                             std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet,
                             service::applet::LibraryAppletMode appletMode)
        : IApplet{state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode} {}

    Result ErrorApplet::Start() {
        auto commonArg{PopNormalInput<service::applet::CommonArguments>()};

        errorStorage = PopNormalInput();
        auto errorCommonHeader{errorStorage->GetSpan().as<ErrorCommonHeader>()};
        Logger::Debug("ErrorApplet: version: 0x{:X}, type: 0x{:X}", commonArg.apiVersion, errorCommonHeader.type);

        switch (errorCommonHeader.type) {
            case ErrorType::ErrorCommonArg:
                HandleErrorCommonArg();
                break;
            case ErrorType::ApplicationErrorArg:
                HandleApplicationErrorArg();
                break;
            default:
                throw exception("ErrorApplet type 0x{:X} is not implemented", errorCommonHeader.type);
        }

        // Notify the guest that we've finished running
        onAppletStateChanged->Signal();

        return {};
    }

    void ErrorApplet::HandleErrorCommonArg() {
        auto errorCommonArg{errorStorage->GetSpan().as<ErrorCommonArg>()};
        Logger::Error("ErrorApplet: error code: 0x{:X}, result: 0x{:X}", errorCommonArg.errorCode, errorCommonArg.result);
    }

    void ErrorApplet::HandleApplicationErrorArg() {
        auto applicationErrorStorage{errorStorage->GetSpan().as<ApplicationErrorArg>()};

        if (applicationErrorStorage.fullscreenMessage[0] == '\0')
            Logger::ErrorNoPrefix("Application Error: {}", applicationErrorStorage.dialogMessage.data());
        else
            Logger::ErrorNoPrefix("Application Error: {}\nFull message: {}", applicationErrorStorage.dialogMessage.data(), applicationErrorStorage.fullscreenMessage.data());
    }

    Result ErrorApplet::GetResult() {
        return {};
    }

    void ErrorApplet::PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) {
        PushNormalInput(data);
    }

    void ErrorApplet::PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) {}
}
