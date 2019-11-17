#include "applet.h"
#include "appletController.h"

namespace skyline::kernel::service::am {
    appletOE::appletOE(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_appletOE, {
        {0x0, SFUNC(appletOE::OpenApplicationProxy)}
    }) {}

    void appletOE::OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationProxy), session, response);
    }

    appletAE::appletAE(const DeviceState& state, ServiceManager& manager) : BaseService(state, manager, false, Service::am_appletAE, {
        {0x64, SFUNC(appletAE::OpenSystemAppletProxy)},
        {0xC8, SFUNC(appletAE::OpenLibraryAppletProxy)},
        {0xC9, SFUNC(appletAE::OpenLibraryAppletProxy)},
        {0x12C, SFUNC(appletAE::OpenOverlayAppletProxy)},
        {0x15E, SFUNC(appletAE::OpenApplicationProxy)}
    }) {}

    void appletAE::OpenLibraryAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ILibraryAppletProxy), session, response);
    }

    void appletAE::OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationProxy), session, response);
    }

    void appletAE::OpenOverlayAppletProxy(type::KSession& session, ipc::IpcRequest& request, ipc::IpcResponse& response) {
        manager.RegisterService(SRVREG(IOverlayAppletProxy), session, response);
    }

    void appletAE::OpenSystemAppletProxy(type::KSession& session, ipc::IpcRequest& request, ipc::IpcResponse& response) {
        manager.RegisterService(SRVREG(ISystemAppletProxy), session, response);
    }

    BaseProxy::BaseProxy(const DeviceState &state, ServiceManager &manager, Service serviceType, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : BaseService(state, manager, false, serviceType, vTable) {}

    void BaseProxy::GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ICommonStateGetter), session, response);
    }

    void BaseProxy::GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISelfController), session, response);
    }

    void BaseProxy::GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IWindowController), session, response);
    }

    void BaseProxy::GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IAudioController), session, response);
    }

    void BaseProxy::GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDisplayController), session, response);
    }

    void BaseProxy::GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ILibraryAppletCreator), session, response);
    }

    void BaseProxy::GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IDebugFunctions), session, response);
    }

    void BaseProxy::GetAppletCommonFunctions(type::KSession& session, ipc::IpcRequest& request, ipc::IpcResponse& response) {
        manager.RegisterService(SRVREG(IAppletCommonFunctions), session, response);
    }

    IApplicationProxy::IApplicationProxy(const DeviceState &state, ServiceManager &manager) : BaseProxy(state, manager, Service::am_IApplicationProxy, {
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

    ILibraryAppletProxy::ILibraryAppletProxy(const DeviceState &state, ServiceManager &manager) : BaseProxy(state, manager, Service::am_ILibraryAppletProxy, {
        {0x0, SFUNC(BaseProxy::GetCommonStateGetter)},
        {0x1, SFUNC(BaseProxy::GetSelfController)},
        {0x2, SFUNC(BaseProxy::GetWindowController)},
        {0x3, SFUNC(BaseProxy::GetAudioController)},
        {0x4, SFUNC(BaseProxy::GetDisplayController)},
        {0xB, SFUNC(BaseProxy::GetLibraryAppletCreator)},
        {0x3E8, SFUNC(BaseProxy::GetDebugFunctions)}
    }) {}

    ISystemAppletProxy::ISystemAppletProxy(const DeviceState& state, ServiceManager& manager) : BaseProxy(state, manager, Service::am_ISystemAppletProxy, {
        {0x0, SFUNC(BaseProxy::GetCommonStateGetter)},
        {0x1, SFUNC(BaseProxy::GetSelfController)},
        {0x2, SFUNC(BaseProxy::GetWindowController)},
        {0x3, SFUNC(BaseProxy::GetAudioController)},
        {0x4, SFUNC(BaseProxy::GetDisplayController)},
        {0xB, SFUNC(BaseProxy::GetLibraryAppletCreator)},
        {0x17, SFUNC(BaseProxy::GetAppletCommonFunctions)},
        {0x3E8, SFUNC(BaseProxy::GetDebugFunctions)}
    }) {}

    IOverlayAppletProxy::IOverlayAppletProxy(const DeviceState& state, ServiceManager& manager) : BaseProxy(state, manager, Service::am_IOverlayAppletProxy, {
        {0x0, SFUNC(BaseProxy::GetCommonStateGetter)},
        {0x1, SFUNC(BaseProxy::GetSelfController)},
        {0x2, SFUNC(BaseProxy::GetWindowController)},
        {0x3, SFUNC(BaseProxy::GetAudioController)},
        {0x4, SFUNC(BaseProxy::GetDisplayController)},
        {0xB, SFUNC(BaseProxy::GetLibraryAppletCreator)},
        {0x15, SFUNC(BaseProxy::GetAppletCommonFunctions)},
        {0x3E8, SFUNC(BaseProxy::GetDebugFunctions)}
    }) {}
}
