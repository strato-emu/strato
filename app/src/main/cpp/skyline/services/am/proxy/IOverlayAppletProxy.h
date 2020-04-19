// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "base_proxy.h"

namespace skyline::service::am {
    /**
     * @brief IOverlayAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#IOverlayAppletProxy)
     */
    class IOverlayAppletProxy : public BaseProxy {
      public:
        IOverlayAppletProxy(const DeviceState &state, ServiceManager &manager);
    };
}
