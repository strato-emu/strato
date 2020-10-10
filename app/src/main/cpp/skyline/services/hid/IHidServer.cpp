// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
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

    Result IHidServer::SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto styleSet{request.Pop<NpadStyleSet>()};
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.styles = styleSet;
        state.input->npad.Update();

        state.logger->Debug("Controller Support:\nPro-Controller: {}\nJoy-Con: Handheld: {}, Dual: {}, L: {}, R: {}\nGameCube: {}\nPokeBall: {}\nNES: {}, NES Handheld: {}, SNES: {}", static_cast<bool>(styleSet.proController), static_cast<bool>(styleSet.joyconHandheld), static_cast<bool>(styleSet.joyconDual), static_cast<bool>(styleSet.joyconLeft), static_cast<bool>
        (styleSet.joyconRight), static_cast<bool>(styleSet.gamecube), static_cast<bool>(styleSet.palma), static_cast<bool>(styleSet.nes), static_cast<bool>(styleSet.nesHandheld), static_cast<bool>(styleSet.snes));
        return {};
    }

    Result IHidServer::GetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(state.input->npad.styles);
        return {};
    }

    Result IHidServer::SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto supportedIds{request.inputBuf.at(0).cast<NpadId>()};
        std::lock_guard lock(state.input->npad.mutex);
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
        request.copyHandles.push_back(state.process->InsertItem(state.input->npad.at(id).updateEvent));
        return {};
    }

    Result IHidServer::GetPlayerLedPattern(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        response.Push<u64>([id] {
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
        std::lock_guard lock(state.input->npad.mutex);
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
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update();
        return {};
    }

    Result IHidServer::SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Dual);
        state.input->npad.Update();
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
                    state.logger->Debug("Vibration #{}&{} - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz - {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", i, i + 1, static_cast<u8>(handle.id), static_cast<u8>(handle.type), values[i].amplitudeLow, values[i].frequencyLow, values[i].amplitudeHigh, values[i].frequencyHigh, values[i + 1].amplitudeLow, values[i + 1].frequencyLow, values[i + 1]
                        .amplitudeHigh, values[i + 1].frequencyHigh);
                    device.Vibrate(values[i], values[i + 1]);
                    i++;
                } else {
                    const auto &value{values[i]};
                    state.logger->Debug("Vibration #{} - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", i, static_cast<u8>(handle.id), static_cast<u8>(handle.type), value.amplitudeLow, value.frequencyLow, value.amplitudeHigh, value.frequencyHigh);
                    device.Vibrate(handle.isRight, value);
                }
            }
        }

        return {};
    }
}
