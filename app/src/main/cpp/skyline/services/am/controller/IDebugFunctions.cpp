#include "IDebugFunctions.h"

namespace skyline::service::am {
    IDebugFunctions::IDebugFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IDebugFunctions, "am:IDebugFunctions", {
    }) {}
}
