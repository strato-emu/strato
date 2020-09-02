// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "IOverlayAppletProxy.h"

namespace skyline::service::am {
    IOverlayAppletProxy::IOverlayAppletProxy(const DeviceState &state, ServiceManager &manager) : BaseProxy(state, manager, {
        {0x0, SFUNC(BaseProxy::GetCommonStateGetter)},
        {0x1, SFUNC(BaseProxy::GetSelfController)},
        {0x2, SFUNC(BaseProxy::GetWindowController)},
        {0x3, SFUNC(BaseProxy::GetAudioController)},
        {0x4, SFUNC(BaseProxy::GetDisplayController)},
        {0xB, SFUNC(BaseProxy::GetLibraryAppletCreator)},
        {0x15, SFUNC(BaseProxy::GetAppletCommonFunctions)},
        {0x3E8, SFUNC(BaseProxy::GetDebugFunctions)}
    }) {}
}
