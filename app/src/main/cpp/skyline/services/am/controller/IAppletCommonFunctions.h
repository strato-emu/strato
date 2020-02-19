#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This contains common various functions (https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions)
     */
    class IAppletCommonFunctions : public BaseService {
      public:
        IAppletCommonFunctions(const DeviceState &state, ServiceManager &manager);
    };
}
