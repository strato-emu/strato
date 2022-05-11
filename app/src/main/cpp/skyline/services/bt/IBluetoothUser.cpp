// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IBluetoothUser.h"

namespace skyline::service::bt {
    IBluetoothUser::IBluetoothUser(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}
}