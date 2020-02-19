#include <services/am/controller/IApplicationFunctions.h>
#include "IApplicationProxy.h"

namespace skyline::service::am {
    IApplicationProxy::IApplicationProxy(const DeviceState &state, ServiceManager &manager) : BaseProxy(state, manager, Service::am_IApplicationProxy, "am:IApplicationProxy", {
        {0x0, SFUNC(BaseProxy::GetCommonStateGetter)},
        {0x1, SFUNC(BaseProxy::GetSelfController)},
        {0x2, SFUNC(BaseProxy::GetWindowController)},
        {0x3, SFUNC(BaseProxy::GetAudioController)},
        {0x4, SFUNC(BaseProxy::GetDisplayController)},
        {0xB, SFUNC(BaseProxy::GetLibraryAppletCreator)},
        {0x14, SFUNC(IApplicationProxy::GetApplicationFunctions)},
        {0x3E8, SFUNC(BaseProxy::GetDebugFunctions)}
    }) {}

    void IApplicationProxy::GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationFunctions), session, response);
    }
}
