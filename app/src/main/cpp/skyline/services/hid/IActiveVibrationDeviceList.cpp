// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <input.h>
#include "IActiveVibrationDeviceList.h"

using namespace skyline::input;

namespace skyline::service::hid {
    IActiveVibrationDeviceList::IActiveVibrationDeviceList(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::hid_IActiveVibrationDeviceList, "hid:IActiveVibrationDeviceList", {
        {0x0, SFUNC(IActiveVibrationDeviceList::ActivateVibrationDevice)}
    }) {}

    void IActiveVibrationDeviceList::ActivateVibrationDevice(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto handle = request.Pop<NpadDeviceHandle>();

        if (!handle.isRight)
            state.input->npad.at(handle.id).vibrationRight = NpadVibrationValue{};
    }
}
