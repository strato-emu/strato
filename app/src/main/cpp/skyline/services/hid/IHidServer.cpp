// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <input.h>
#include "IHidServer.h"
#include "IActiveVibrationDeviceList.h"

using namespace skyline::input;

namespace skyline::service::hid {
    IHidServer::IHidServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IHidServer::CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAppletResource), session, response);
        return {};
    }

    Result IHidServer::ActivateDebugPad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IHidServer::ActivateTouchScreen(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->touch.Activate();
        return {};
    }

    Result IHidServer::ActivateMouse(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IHidServer::ActivateKeyboard(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IHidServer::StartSixAxisSensor(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IHidServer::StopSixAxisSensor(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IHidServer::SetGyroscopeZeroDriftMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto npadHandle{request.Pop<NpadDeviceHandle>()};
        auto mode{request.Pop<GyroscopeZeroDriftMode>()};

        state.input->npad[npadHandle.id].gyroZeroDriftMode = mode;
        return {};
    }

    Result IHidServer::GetGyroscopeZeroDriftMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto npadHandle{request.Pop<NpadDeviceHandle>()};

        response.Push(state.input->npad[npadHandle.id].gyroZeroDriftMode);
        return {};
    }

    Result IHidServer::ResetGyroscopeZeroDriftMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto npadHandle{request.Pop<NpadDeviceHandle>()};

        state.input->npad[npadHandle.id].gyroZeroDriftMode = GyroscopeZeroDriftMode::Standard;
        return {};
    }

    Result IHidServer::IsSixAxisSensorAtRest(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(1);
        return {};
    }

    Result IHidServer::SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto styleSet{request.Pop<NpadStyleSet>()};
        std::scoped_lock lock{state.input->npad.mutex};
        state.input->npad.styles = styleSet;
        state.input->npad.Update();

        Logger::Debug("Controller Support:\nPro-Controller: {}\nJoy-Con: Handheld: {}, Dual: {}, L: {}, R: {}\nGameCube: {}\nPokeBall: {}\nNES: {}, NES Handheld: {}, SNES: {}", static_cast<bool>(styleSet.proController), static_cast<bool>(styleSet.joyconHandheld), static_cast<bool>(styleSet.joyconDual), static_cast<bool>(styleSet.joyconLeft), static_cast<bool>
        (styleSet.joyconRight), static_cast<bool>(styleSet.gamecube), static_cast<bool>(styleSet.palma), static_cast<bool>(styleSet.nes), static_cast<bool>(styleSet.nesHandheld), static_cast<bool>(styleSet.snes));
        return {};
    }

    Result IHidServer::GetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(state.input->npad.styles);
        return {};
    }

    Result IHidServer::SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto supportedIds{request.inputBuf.at(0).cast<NpadId>()};
        std::scoped_lock lock{state.input->npad.mutex};
        state.input->npad.supportedIds.assign(supportedIds.begin(), supportedIds.end());
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::ActivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->npad.Activate();
        return {};
    }

    Result IHidServer::DeactivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->npad.Deactivate();
        return {};
    }

    Result IHidServer::AcquireNpadStyleSetUpdateEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        auto handle{state.process->InsertItem(state.input->npad.at(id).updateEvent)};

        state.input->npad.at(id).updateEvent->Signal();

        Logger::Debug("Npad {} Style Set Update Event Handle: 0x{:X}", id, handle);
        response.copyHandles.push_back(handle);
        return {};
    }

    Result IHidServer::GetPlayerLedPattern(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        response.Push<u64>([id]() -> u64 {
            switch (id) {
                case NpadId::Player1:
                    return 0b0001;
                case NpadId::Player2:
                    return 0b0011;
                case NpadId::Player3:
                    return 0b0111;
                case NpadId::Player4:
                    return 0b1111;
                case NpadId::Player5:
                    return 0b1001;
                case NpadId::Player6:
                    return 0b0101;
                case NpadId::Player7:
                    return 0b1101;
                case NpadId::Player8:
                    return 0b0110;
                default:
                    return 0b0000;
            }
        }());
        return {};
    }

    Result IHidServer::ActivateNpadWithRevision(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->npad.Activate();
        return {};
    }

    Result IHidServer::SetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::scoped_lock lock{state.input->npad.mutex};
        request.Skip<u64>();
        state.input->npad.orientation = request.Pop<NpadJoyOrientation>();
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::GetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(state.input->npad.orientation);
        return {};
    }

    Result IHidServer::SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        std::scoped_lock lock{state.input->npad.mutex};
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        std::scoped_lock lock{state.input->npad.mutex};
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        std::scoped_lock lock{state.input->npad.mutex};
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Dual);
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::StartLrAssignmentMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // This isn't really necessary for us due to input preconfiguration so stub it
        return {};
    }

    Result IHidServer::StopLrAssignmentMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IHidServer::SetNpadHandheldActivationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u64>();
        auto activationMode{request.Pop<NpadHandheldActivationMode>()};

        std::scoped_lock lock{state.input->npad.mutex};
        state.input->npad.handheldActivationMode = activationMode;
        return {};
    }

    Result IHidServer::GetNpadHandheldActivationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::scoped_lock lock{state.input->npad.mutex};
        response.Push(state.input->npad.handheldActivationMode);
        return {};
    }


    Result IHidServer::GetVibrationDeviceInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto deviceHandle{request.Pop<NpadDeviceHandle>()};
        auto id{deviceHandle.id};

        if (id > NpadId::Player8 && id != NpadId::Handheld && id != NpadId::Unknown)
            return result::InvalidNpadId;

        auto vibrationDeviceType{NpadVibrationDeviceType::Unknown};
        auto vibrationDevicePosition{NpadVibrationDevicePosition::None};

        if (deviceHandle.GetType() == NpadControllerType::Gamecube)
            vibrationDeviceType = NpadVibrationDeviceType::EccentricRotatingMass;
        else
            vibrationDeviceType = NpadVibrationDeviceType::LinearResonantActuator;

        if (vibrationDeviceType == NpadVibrationDeviceType::LinearResonantActuator)
            if (deviceHandle.isRight)
                vibrationDevicePosition = NpadVibrationDevicePosition::Right;
            else
                vibrationDevicePosition = NpadVibrationDevicePosition::Left;

        response.Push(NpadVibrationDeviceInfo{vibrationDeviceType, vibrationDevicePosition});

        return {};
    }

    Result IHidServer::SendVibrationValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto &handle{request.Pop<NpadDeviceHandle>()};
        auto &device{state.input->npad.at(handle.id)};
        if (device.type == handle.GetType()) {
            const auto &value{request.Pop<NpadVibrationValue>()};
            Logger::Debug("Vibration - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", static_cast<u8>(handle.id), static_cast<u8>(handle.type), value.amplitudeLow, value.frequencyLow, value.amplitudeHigh, value.frequencyHigh);
            device.VibrateSingle(handle.isRight, value);
        }

        return {};
    }

    Result IHidServer::CreateActiveVibrationDeviceList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IActiveVibrationDeviceList), session, response);
        return {};
    }

    Result IHidServer::SendVibrationValues(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u64>(); // appletResourceUserId

        auto handles{request.inputBuf.at(0).cast<NpadDeviceHandle>()};
        auto values{request.inputBuf.at(1).cast<NpadVibrationValue>()};

        for (size_t i{}; i < handles.size(); ++i) {
            const auto &handle{handles[i]};
            auto &device{state.input->npad.at(handle.id)};
            if (device.type == handle.GetType()) {
                if (i + 1 != handles.size() && handles[i + 1].id == handle.id && handles[i + 1].isRight && !handle.isRight) {
                    Logger::Debug("Vibration #{}&{} - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz - {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", i, i + 1, static_cast<u8>(handle.id), static_cast<u8>(handle.type), values[i].amplitudeLow, values[i].frequencyLow, values[i].amplitudeHigh, values[i].frequencyHigh, values[i + 1].amplitudeLow, values[i + 1].frequencyLow, values[i + 1]
                        .amplitudeHigh, values[i + 1].frequencyHigh);
                    device.Vibrate(values[i], values[i + 1]);
                    i++;
                } else {
                    const auto &value{values[i]};
                    Logger::Debug("Vibration #{} - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", i, static_cast<u8>(handle.id), static_cast<u8>(handle.type), value.amplitudeLow, value.frequencyLow, value.amplitudeHigh, value.frequencyHigh);
                    device.VibrateSingle(handle.isRight, value);
                }
            }
        }

        return {};
    }

    Result IHidServer::IsVibrationPermitted(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u8>(0);
        return {};
    }

    Result IHidServer::SetPalmaBoostMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }
}
