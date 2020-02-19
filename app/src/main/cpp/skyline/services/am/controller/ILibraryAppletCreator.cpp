#include "ILibraryAppletCreator.h"

namespace skyline::service::am {
    ILibraryAppletCreator::ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_ILibraryAppletCreator, "am:ILibraryAppletCreator", {
    }) {}
}
