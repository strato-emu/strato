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
    };
}
