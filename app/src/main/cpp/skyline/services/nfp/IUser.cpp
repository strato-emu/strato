// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "IUserManager.h"
#include "IUser.h"

namespace skyline::service::nfp {
    IUser::IUser(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager), attachAvailabilityChangeEvent(std::make_shared<type::KEvent>(state, false)) {}

    Result IUser::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        nfpState = State::Initialized;
        return {};
    }

    Result IUser::ListDevices(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IUser::GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(nfpState);
        return {};
    }

    Result IUser::AttachAvailabilityChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem(attachAvailabilityChangeEvent)};
        Logger::Debug("Attach Availability Change Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);

        return {};
    }
}
