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
        {0x3,   SFunc(IApplicationProxy::GetAudioController)},
        {0x4,   SFunc(IApplicationProxy::GetDisplayController)},
        {0xB,   SFunc(IApplicationProxy::GetLibraryAppletCreator)},
        {0x14,  SFunc(IApplicationProxy::GetApplicationFunctions)},
        {0x3E8, SFunc(IApplicationProxy::GetDisplayController)}
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

    void IApplicationProxy::GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IAudioController, session, response);
    }

    void IApplicationProxy::GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IDisplayController, session, response);
    }

    void IApplicationProxy::GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_ILibraryAppletCreator, session, response);
    }

    void IApplicationProxy::GetApplicationFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IApplicationFunctions, session, response);
    }

    void IApplicationProxy::IDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::am_IDebugFunctions, session, response);
    }

    ICommonStateGetter::ICommonStateGetter(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_ICommonStateGetter, {
        {0x0, SFunc(ICommonStateGetter::GetEventHandle)},
        {0x9, SFunc(ICommonStateGetter::GetCurrentFocusState)},
        {0x5, SFunc(ICommonStateGetter::GetOperationMode)},
        {0x6, SFunc(ICommonStateGetter::GetPerformanceMode)}
    }) {
        operationMode = static_cast<OperationMode>(state.settings->GetBool("operation_mode"));
        state.logger->Write(Logger::Info, "Switch on mode: {}", static_cast<bool>(operationMode) ? "Docked" : "Handheld");
    }

    void ICommonStateGetter::GetEventHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        messageEvent = state.thisProcess->NewHandle<type::KEvent>();
        response.copyHandles.push_back(messageEvent->handle);
    }

    void ICommonStateGetter::GetCurrentFocusState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue<u8>(static_cast<u8>(ApplicationStatus::InFocus));
    }

    void ICommonStateGetter::GetOperationMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue<u8>(static_cast<u8>(operationMode));
    }

    void ICommonStateGetter::GetPerformanceMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue<u32>(static_cast<u32>(operationMode));
    }

    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_ISelfController, {
        {0xB, SFunc(ISelfController::SetOperationModeChangedNotification)},
        {0xC, SFunc(ISelfController::SetPerformanceModeChangedNotification)},
        {0xD, SFunc(ISelfController::SetFocusHandlingMode)}
    }) {}

    void ISelfController::SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    IWindowController::IWindowController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IWindowController, {
        {0x1, SFunc(IWindowController::GetAppletResourceUserId)},
        {0xA, SFunc(IWindowController::AcquireForegroundRights)}
    }) {}

    void IWindowController::GetAppletResourceUserId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue(static_cast<u64>(state.thisProcess->mainThread));
    }

    void IWindowController::AcquireForegroundRights(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    IAudioController::IAudioController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IAudioController, {
    }) {}

    IDisplayController::IDisplayController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IDisplayController, {
    }) {}

    ILibraryAppletCreator::ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_ILibraryAppletCreator, {
    }) {}

    IApplicationFunctions::IApplicationFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IApplicationFunctions, {
        {0x28, SFunc(IApplicationFunctions::NotifyRunning)}
    }) {}

    void IApplicationFunctions::NotifyRunning(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue<u8>(1);
    }

    IDebugFunctions::IDebugFunctions(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::am_IDebugFunctions, {
    }) {}
}
