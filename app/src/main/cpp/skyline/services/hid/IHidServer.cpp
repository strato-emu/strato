// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "IHidServer.h"
#include "IActiveVibrationDeviceList.h"

using namespace skyline::input;

namespace skyline::service::hid {
    IHidServer::IHidServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::hid_IHidServer, "hid:IHidServer", {
        {0x0, SFUNC(IHidServer::CreateAppletResource)},
        {0x64, SFUNC(IHidServer::SetSupportedNpadStyleSet)},
        {0x64, SFUNC(IHidServer::GetSupportedNpadStyleSet)},
        {0x66, SFUNC(IHidServer::SetSupportedNpadIdType)},
        {0x67, SFUNC(IHidServer::ActivateNpad)},
        {0x68, SFUNC(IHidServer::DeactivateNpad)},
        {0x6A, SFUNC(IHidServer::AcquireNpadStyleSetUpdateEventHandle)},
        {0x6D, SFUNC(IHidServer::ActivateNpadWithRevision)},
        {0x78, SFUNC(IHidServer::SetNpadJoyHoldType)},
        {0x79, SFUNC(IHidServer::GetNpadJoyHoldType)},
        {0x7A, SFUNC(IHidServer::SetNpadJoyAssignmentModeSingleByDefault)},
        {0x7B, SFUNC(IHidServer::SetNpadJoyAssignmentModeSingle)},
        {0x7C, SFUNC(IHidServer::SetNpadJoyAssignmentModeDual)},
        {0xCB, SFUNC(IHidServer::CreateActiveVibrationDeviceList)},
        {0xCE, SFUNC(IHidServer::SendVibrationValues)}
    }) {}

    void IHidServer::CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAppletResource), session, response);
    }

    void IHidServer::SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto styleSet = request.Pop<NpadStyleSet>();
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.styles = styleSet;
        state.input->npad.Update();

        state.logger->Debug("Controller Support:\nPro-Controller: {}\nJoy-Con: Handheld: {}, Dual: {}, L: {}, R: {}\nGameCube: {}\nPokeBall: {}\nNES: {}, NES Handheld: {}, SNES: {}", static_cast<bool>(styleSet.proController), static_cast<bool>(styleSet.joyconHandheld), static_cast<bool>(styleSet.joyconDual), static_cast<bool>(styleSet.joyconLeft), static_cast<bool>
        (styleSet.joyconRight), static_cast<bool>(styleSet.gamecube), static_cast<bool>(styleSet.palma), static_cast<bool>(styleSet.nes), static_cast<bool>(styleSet.nesHandheld), static_cast<bool>(styleSet.snes));
    }

    void IHidServer::GetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(state.input->npad.styles);
    }

    void IHidServer::SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto &buffer = request.inputBuf.at(0);
        u64 address = buffer.address;
        size_t size = buffer.size / sizeof(NpadId);

        std::vector<NpadId> supportedIds(size);
        for (size_t i = 0; i < size; i++) {
            supportedIds[i] = state.process->GetObject<NpadId>(address);
            address += sizeof(NpadId);
        }

        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.supportedIds = supportedIds;
        state.input->npad.Update();
    }

    void IHidServer::ActivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->npad.Activate();
    }

    void IHidServer::DeactivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->npad.Deactivate();
    }

    void IHidServer::AcquireNpadStyleSetUpdateEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        request.copyHandles.push_back(state.process->InsertItem(state.input->npad.at(id).updateEvent));
    }

    void IHidServer::ActivateNpadWithRevision(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.input->npad.Activate();
    }

    void IHidServer::SetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::lock_guard lock(state.input->npad.mutex);
        request.Skip<u64>();
        state.input->npad.orientation = request.Pop<NpadJoyOrientation>();
        state.input->npad.Update();
    }

    void IHidServer::GetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(state.input->npad.orientation);
    }

    void IHidServer::SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update();
    }

    void IHidServer::SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update();
    }

    void IHidServer::SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        std::lock_guard lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Dual);
        state.input->npad.Update();
    }

    void IHidServer::CreateActiveVibrationDeviceList(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IActiveVibrationDeviceList), session, response);
    }

    void IHidServer::SendVibrationValues(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u64>(); // appletResourceUserId

        auto &handleBuf = request.inputBuf.at(0);
        std::span handles(reinterpret_cast<NpadDeviceHandle *>(handleBuf.address), handleBuf.size / sizeof(NpadDeviceHandle));
        auto &valueBuf = request.inputBuf.at(1);
        std::span values(reinterpret_cast<NpadVibrationValue *>(valueBuf.address), valueBuf.size / sizeof(NpadVibrationValue));

        for (int i = 0; i < handles.size(); ++i) {
            auto &handle = handles[i];

            auto &device = state.input->npad.at(handle.id);
            if (device.type == handle.GetType()) {
                if (i + 1 != handles.size() && handles[i + 1].id == handle.id && handles[i + 1].isRight && !handle.isRight) {
                    state.logger->Info("Vibration #{}&{} - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz - {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", i, i + 1, u8(handle.id), u8(handle.type), values[i].amplitudeLow, values[i].frequencyLow, values[i].amplitudeHigh, values[i].frequencyHigh, values[i + 1].amplitudeLow, values[i + 1].frequencyLow, values[i + 1].amplitudeHigh, values[i + 1].frequencyHigh);
                    device.Vibrate(values[i], values[i + 1]);
                    i++;
                } else {
                    auto &value = values[i];
                    state.logger->Info("Vibration #{} - Handle: 0x{:02X} (0b{:05b}), Vibration: {:.2f}@{:.2f}Hz, {:.2f}@{:.2f}Hz", i, u8(handle.id), u8(handle.type), value.amplitudeLow, value.frequencyLow, value.amplitudeHigh, value.frequencyHigh);
                    device.Vibrate(handle.isRight, value);
                }
            }
        }
    }
}
