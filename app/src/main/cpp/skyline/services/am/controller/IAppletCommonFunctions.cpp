#include "IAppletCommonFunctions.h"

namespace skyline::service::am {
    IAppletCommonFunctions::IAppletCommonFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IAppletCommonFunctions, "am:IAppletCommonFunctions", {
    }) {}
}
