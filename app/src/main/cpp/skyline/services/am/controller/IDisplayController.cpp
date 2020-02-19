#include "IDisplayController.h"

namespace skyline::service::am {
    IDisplayController::IDisplayController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IDisplayController, "am:IDisplayController", {
    }) {}
}
