#include "appletOE.h"

namespace skyline::kernel::service::am {
    appletOE::appletOE(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::am_appletOE, {
        {0x0, SFunc(appletOE::OpenApplicationProxy)}
    }) {}

    void appletOE::OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IApplicationProxy, session, response);
    }

    IApplicationProxy::IApplicationProxy(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::am_IApplicationProxy, {
        {0x0,   SFunc(IApplicationProxy::GetCommonStateGetter)},
        {0x1,   SFunc(IApplicationProxy::GetSelfController)},
        {0x2,   SFunc(IApplicationProxy::GetWindowController)},
        {0xB,   SFunc(IApplicationProxy::GetLibraryAppletCreator)},
        {0x14,  SFunc(IApplicationProxy::GetApplicationFunctions)}
    }) {}

    void IApplicationProxy::GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_ICommonStateGetter, session, response);
    }

    void IApplicationProxy::GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_ISelfController, session, response);
    }

    void IApplicationProxy::GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IWindowController, session, response);
    }

    void IApplicationProxy::GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_ILibraryAppletCreator, session, response);
    }

    void IApplicationProxy::GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IApplicationFunctions, session, response);
    }

    ICommonStateGetter::ICommonStateGetter(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_ICommonStateGetter, {
    }) {}

    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_ISelfController, {
    }) {}

    IWindowController::IWindowController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IWindowController, {
    }) {}

    ILibraryAppletCreator::ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_ILibraryAppletCreator, {
    }) {}

    IApplicationFunctions::IApplicationFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IApplicationFunctions, {
    }) {}
}
