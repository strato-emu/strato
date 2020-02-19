#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief This has functions relating to volume control (https://switchbrew.org/wiki/Applet_Manager_services#IAudioController)
     */
    class IAudioController : public BaseService {
      public:
        IAudioController(const DeviceState &state, ServiceManager &manager);
    };
}
