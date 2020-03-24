#include "IManagerRootService.h"
#include <kernel/types/KProcess.h>
#include "IApplicationDisplayService.h"

namespace skyline::service::visrv {
    IManagerRootService::IManagerRootService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::visrv_IManagerRootService, "visrv:IManagerRootService", {
        {0x2, SFUNC(IManagerRootService::GetDisplayService)}
    }) {}

    void IManagerRootService::GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationDisplayService), session, response);
    }
}
