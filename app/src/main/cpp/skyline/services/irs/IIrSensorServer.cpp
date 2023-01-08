// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IIrSensorServer.h"
#include <input.h>
#include <kernel/types/KProcess.h>
#include "iirsensor_core.h"
using namespace skyline::input;
namespace skyline::service::irs {
    IIrSensorServer::IIrSensorServer(const DeviceState &state, ServiceManager &manager, SharedIirCore &core) : BaseService(state, manager), core(core) {}

    Result IIrSensorServer::GetNpadIrCameraHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto id{request.Pop<NpadId>()};
        if (id > NpadId::Player8 && id != NpadId::Handheld && id != NpadId::Unknown)
            return result::InvalidNpadId;

        struct IrCameraHandle {
            u8 npadIndex;
            u8 npadType;
            u8 _pad0_[2];
        } handle{
            .npadIndex = static_cast<u8>(state.input->npad.NpadIdToIndex(id)),
        };

        response.Push(handle);

        return {};
    }

    Result IIrSensorServer::ActivateIrsensorWithFunctionLevel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IIrSensorServer::GetIrsensorSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle{state.process->InsertItem<type::KSharedMemory>(core.sharedIirMemory)};

        response.copyHandles.push_back(handle);
        return {};
    }

}
