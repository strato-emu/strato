// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2020 yuzu Emulator Project (https://github.com/yuzu-emu/)

#include <input.h>
#include <input/npad.h>
#include <services/applet/common_arguments.h>
#include <services/am/storage/ObjIStorage.h>
#include "controller_applet.h"

namespace skyline::applet {
    ControllerApplet::ControllerApplet(const DeviceState &state,
                                       service::ServiceManager &manager,
                                       std::shared_ptr<kernel::type::KEvent> onAppletStateChanged,
                                       std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet,
                                       std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet,
                                       service::applet::LibraryAppletMode appletMode)
        : IApplet{state, manager, std::move(onAppletStateChanged), std::move(onNormalDataPushFromApplet), std::move(onInteractiveDataPushFromApplet), appletMode} {}

    void ControllerApplet::HandleShowControllerSupport(input::NpadStyleSet styleSet, ControllerAppletVersion version, span<u8> arg) {
        // Generic macro due to both versions of arguments sharing the same fields but having different layouts
        auto handle{[&](auto controllerSupportModeArg) {
            Logger::InfoNoPrefix("Controller Support: "
                                 "Player Count: {} - {}, "
                                 "Take Over Connection: {}, Left Justify: {}, Dual Joy-Con Allowed: {}, Single Mode Enabled: {}, "
                                 "Identification Color Enabled: {}, Explain Text Enabled: {}",
                                 controllerSupportModeArg.playerCountMin, controllerSupportModeArg.playerCountMax,
                                 controllerSupportModeArg.enableTakeOverConnection, controllerSupportModeArg.enableLeftJustify, controllerSupportModeArg.enablePermitJoyDual, controllerSupportModeArg.enableSingleMode,
                                 controllerSupportModeArg.enableIdentificationColor, controllerSupportModeArg.enableExplainText);

            // Here is where we would trigger the applet UI

            auto &npad{state.input->npad};
            std::scoped_lock lock{npad.mutex};

            PushNormalDataAndSignal(std::make_shared<service::am::ObjIStorage<ControllerSupportResultInfo>>(state, manager, ControllerSupportResultInfo{
                .playerCount = static_cast<i8>(controllerSupportModeArg.enableSingleMode ? 1 : npad.GetConnectedControllerCount()),
                .selectedId = [&npad]() {
                    if (npad.controllers[0].device) {
                        return npad.controllers[0].device->id;
                    } else {
                        Logger::Warn("Controller requested but none connected!");
                        return input::NpadId::Player1; // Fallback to player 1
                    }
                }(),
                .result = {}
            }));
        }};

        switch (version) {
            case ControllerAppletVersion::Version3:
            case ControllerAppletVersion::Version4:
            case ControllerAppletVersion::Version5:
                handle(arg.as<ControllerSupportArgOld>());
                break;
            case ControllerAppletVersion::Version7:
            case ControllerAppletVersion::Version8:
                handle(arg.as<ControllerSupportArgNew>());
                break;
            default:
                Logger::Warn("Unsupported controller applet version {}", static_cast<u32>(version));
                break;
        }
    }

    Result ControllerApplet::Start() {
        auto commonArg{PopNormalInput<service::applet::CommonArguments>()};
        ControllerAppletVersion appletVersion{commonArg.apiVersion};

        auto argPrivate{PopNormalInput<ControllerSupportArgPrivate>()};

        // Some games such as Cave Story+ set invalid values for the ControllerSupportMode so use argSize to derive it if necessary (from yuzu)
        if (argPrivate.mode >= ControllerSupportMode::MaxControllerSupportMode) {
            switch (argPrivate.argSize) {
                case sizeof(ControllerSupportArgOld):
                case sizeof(ControllerSupportArgNew):
                    argPrivate.mode = ControllerSupportMode::ShowControllerSupport;
                    break;
                default:
                    // TODO: when we support other modes make sure to add them here too
                    break;
            }
        }

        std::scoped_lock lock{normalInputDataMutex};
        switch (argPrivate.mode) {
            case ControllerSupportMode::ShowControllerSupport:
                HandleShowControllerSupport(argPrivate.styleSet, appletVersion, normalInputData.front()->GetSpan());
                normalInputData.pop();
                break;
            default:
                Logger::Warn("Controller applet mode {} is unimplemented", static_cast<u32>(argPrivate.mode));
                normalInputData.pop();

                // Return empty result
                PushNormalDataAndSignal(std::make_shared<service::am::ObjIStorage<Result>>(state, manager, Result{}));
                break;
        }

        // Notify the guest that we've finished running
        onAppletStateChanged->Signal();
        return {};
    }

    Result ControllerApplet::GetResult() {
        return {};
    }

    void ControllerApplet::PushNormalDataToApplet(std::shared_ptr<service::am::IStorage> data) {
        PushNormalInput(data);
    }

    void ControllerApplet::PushInteractiveDataToApplet(std::shared_ptr<service::am::IStorage> data) {}
}
