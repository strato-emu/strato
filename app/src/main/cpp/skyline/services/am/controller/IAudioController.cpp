#include "IAudioController.h"

namespace skyline::service::am {
    IAudioController::IAudioController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IAudioController, "am:IAudioController", {
    }) {}
}
