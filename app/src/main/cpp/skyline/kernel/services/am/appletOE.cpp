#include "appletOE.h"

namespace skyline::kernel::service::am {
    appletOE::appletOE(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::apm, {
        {0x0, SFunc(appletOE::OpenApplicationProxy)}
    }) {}

    void appletOE::OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IApplicationProxy, session, response);
    }

    IApplicationProxy::IApplicationProxy(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::am_IApplicationProxy, {
        {0x0, SFunc(IApplicationProxy::GetCommonStateGetter)}
    }) {}

    void IApplicationProxy::GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // TODO: This
    }
}
