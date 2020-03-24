#include "ISystemDisplayService.h"

namespace skyline::service::visrv {
    ISystemDisplayService::ISystemDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, Service::visrv_ISystemDisplayService, "visrv:ISystemDisplayService", {
        {0x89D, SFUNC(ISystemDisplayService::SetLayerZ)},
        {0x908, SFUNC(IDisplayService::CreateStrayLayer)}
    }) {}

    void ISystemDisplayService::SetLayerZ(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}
}
