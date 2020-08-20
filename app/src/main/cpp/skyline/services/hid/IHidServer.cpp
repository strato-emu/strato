// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "IHidServer.h"

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
        {0x7C, SFUNC(IHidServer::SetNpadJoyAssignmentModeDual)}
    }) {}

    void IHidServer::CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAppletResource), session, response);
    }

    void IHidServer::SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto styleSet = request.Pop<NpadStyleSet>();
        std::unique_lock lock(state.input->npad.mutex);
        state.input->npad.styles = styleSet;
        state.input->npad.Update(lock);

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
        std::vector<NpadId> supportedIds;

        for (size_t i = 0; i < size; i++) {
            supportedIds.push_back(state.process->GetObject<NpadId>(address));
            address += sizeof(NpadId);
        }

        std::unique_lock lock(state.input->npad.mutex);
        state.input->npad.supportedIds = supportedIds;
        state.input->npad.Update(lock);
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
        std::unique_lock lock(state.input->npad.mutex);
        request.Skip<u64>();
        state.input->npad.orientation = request.Pop<NpadJoyOrientation>();
        state.input->npad.Update(lock);
    }

    void IHidServer::GetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(state.input->npad.orientation);
    }

    void IHidServer::SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        std::unique_lock lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update(lock);
    }

    void IHidServer::SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        std::unique_lock lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Single);
        state.input->npad.Update(lock);
    }

    void IHidServer::SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id = request.Pop<NpadId>();
        std::unique_lock lock(state.input->npad.mutex);
        state.input->npad.at(id).SetAssignment(NpadJoyAssignment::Dual);
        state.input->npad.Update(lock);
    }
}
