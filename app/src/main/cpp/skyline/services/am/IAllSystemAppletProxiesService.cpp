#include "proxy/ILibraryAppletProxy.h"
#include "proxy/IApplicationProxy.h"
#include "proxy/IOverlayAppletProxy.h"
#include "proxy/ISystemAppletProxy.h"
#include "IAllSystemAppletProxiesService.h"

namespace skyline::service::am {
    IAllSystemAppletProxiesService::IAllSystemAppletProxiesService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_IAllSystemAppletProxiesService, "am:IAllSystemAppletProxiesService", {
        {0x64, SFUNC(IAllSystemAppletProxiesService::OpenSystemAppletProxy)},
        {0xC8, SFUNC(IAllSystemAppletProxiesService::OpenLibraryAppletProxy)},
        {0xC9, SFUNC(IAllSystemAppletProxiesService::OpenLibraryAppletProxy)},
        {0x12C, SFUNC(IAllSystemAppletProxiesService::OpenOverlayAppletProxy)},
        {0x15E, SFUNC(IAllSystemAppletProxiesService::OpenApplicationProxy)}
    }) {}

    void IAllSystemAppletProxiesService::OpenLibraryAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ILibraryAppletProxy), session, response);
    }

    void IAllSystemAppletProxiesService::OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationProxy), session, response);
    }

    void IAllSystemAppletProxiesService::OpenOverlayAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IOverlayAppletProxy), session, response);
    }

    void IAllSystemAppletProxiesService::OpenSystemAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISystemAppletProxy), session, response);
    }
}
