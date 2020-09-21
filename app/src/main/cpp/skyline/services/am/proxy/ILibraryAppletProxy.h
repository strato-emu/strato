// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "base_proxy.h"

namespace skyline::service::am {
    /**
     * @brief ILibraryAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletProxy)
     */
    class ILibraryAppletProxy : public BaseProxy {
      public:
        ILibraryAppletProxy(const DeviceState &state, ServiceManager &manager);

        SERVICE_DECL(
            SFUNC(0x0, BaseProxy, GetCommonStateGetter),
            SFUNC(0x1, BaseProxy, GetSelfController),
            SFUNC(0x2, BaseProxy, GetWindowController),
            SFUNC(0x3, BaseProxy, GetAudioController),
            SFUNC(0x4, BaseProxy, GetDisplayController),
            SFUNC(0xB, BaseProxy, GetLibraryAppletCreator),
            SFUNC(0x3E8, BaseProxy, GetDebugFunctions)
        )
    };
}
