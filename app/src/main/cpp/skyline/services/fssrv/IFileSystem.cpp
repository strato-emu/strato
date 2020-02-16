#include "IFileSystem.h"

namespace skyline::service::fssrv {
    IFileSystem::IFileSystem(const FsType type, const DeviceState &state, ServiceManager &manager) : type(type), BaseService(state, manager, Service::fssrv_IFileSystem, "fssrv:IFileSystem", {}) {}
}
