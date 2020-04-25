// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "IHidServer.h"

using namespace skyline::input;

namespace skyline::service::hid {
    IHidServer::IHidServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::hid_IHidServer, "hid:IHidServer", {
        {0x0, SFUNC(IHidServer::CreateAppletResource)},
        {0x64, SFUNC(IHidServer::SetSupportedNpadStyleSet)},
        {0x66, SFUNC(IHidServer::SetSupportedNpadIdType)},
        {0x67, SFUNC(IHidServer::ActivateNpad)},
        {0x78, SFUNC(IHidServer::SetNpadJoyHoldType)},
        {0x7A, SFUNC(IHidServer::SetNpadJoyAssignmentModeSingleByDefault)},
        {0x7B, SFUNC(IHidServer::SetNpadJoyAssignmentModeSingle)},
        {0x7C, SFUNC(IHidServer::SetNpadJoyAssignmentModeDual)}
    }) {
        state.input->commonNpad->Activate();
    }

    void IHidServer::CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAppletResource), session, response);
    }

    void IHidServer::SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto styleSet = request.Pop<npad::NpadStyleSet>();

        state.logger->Debug("Controller Support:\nPro-Controller: {}\nJoy-Con: Handheld: {}, Dual: {}, L: {}, R: {}\nGameCube: {}\nPokeBall: {}\nNES: {}, NES Handheld: {}, SNES: {}", static_cast<bool>(styleSet.proController), static_cast<bool>(styleSet.joyconHandheld), static_cast<bool>(styleSet.joyconDual), static_cast<bool>(styleSet.joyconLeft), static_cast<bool>
        (styleSet.joyconRight), static_cast<bool>(styleSet.gamecube), static_cast<bool>(styleSet.palma), static_cast<bool>(styleSet.nes), static_cast<bool>(styleSet.nesHandheld), static_cast<bool>(styleSet.snes));
    }

    void IHidServer::SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto &buffer = request.inputBuf.at(0);
        size_t numId = buffer.size / sizeof(npad::NpadId);
        u64 address = buffer.address;

        for (size_t i = 0; i < numId; i++) {
            auto id = state.process->GetObject<npad::NpadId>(address);
            state.input->npad.at(NpadIdToIndex(id))->supported = true;

            address += sizeof(npad::NpadId);
        }
    }

    void IHidServer::ActivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void IHidServer::SetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto appletResourceUID = request.Pop<u64>();
        state.input->commonNpad->orientation = request.Pop<npad::NpadJoyOrientation>();
    }

    void IHidServer::SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<npad::NpadId>();
        state.input->npad.at(npad::NpadIdToIndex(id))->SetAssignment(npad::NpadJoyAssignment::Single);
    }

    void IHidServer::SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<npad::NpadId>();
        state.input->npad.at(npad::NpadIdToIndex(id))->SetAssignment(npad::NpadJoyAssignment::Single);
    }

    void IHidServer::SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<npad::NpadId>();
        state.input->npad.at(npad::NpadIdToIndex(id))->SetAssignment(npad::NpadJoyAssignment::Dual);
    }
}
