#pragma once

#include "base_proxy.h"

namespace skyline::service::am {
    /**
     * @brief ISystemAppletProxy returns handles to various services (https://switchbrew.org/wiki/Applet_Manager_services#ISystemAppletProxy)
     */
    class ISystemAppletProxy : public BaseProxy {
      public:
        ISystemAppletProxy(const DeviceState &state, ServiceManager &manager);
    };
}
