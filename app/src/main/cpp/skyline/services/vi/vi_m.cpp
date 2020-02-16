#include "vi_m.h"
#include <kernel/types/KProcess.h>
#include <services/nvnflinger/dispdrv.h>
#include <gpu/display.h>

namespace skyline::service::vi {
    vi_m::vi_m(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::nvdrv, "nvdrv", {
        {0x2, SFUNC(vi_m::GetDisplayService)}
    }) {}

    void vi_m::GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IApplicationDisplayService), session, response);
    }

    IDisplayService::IDisplayService(const DeviceState &state, ServiceManager &manager, const Service serviceType, const std::string &serviceName, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable) : BaseService(state, manager, serviceType, serviceName, vTable) {}

    void IDisplayService::CreateStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Creating Stray Layer");
        response.Push<u64>(0); // There's only one layer
        gpu::Parcel parcel(state);
        LayerParcel data{
            .type = 0x20,
            .pid = 0,
            .bufferId = 0, // As we only have one layer and buffer
            .string = "dispdrv"
        };
        parcel.WriteData(data);
        response.Push<u64>(parcel.WriteParcel(request.outputBuf.at(0)));
    }

    IApplicationDisplayService::IApplicationDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, Service::vi_IApplicationDisplayService, "vi:IApplicationDisplayService", {
        {0x64, SFUNC(IApplicationDisplayService::GetRelayService)},
        {0x65, SFUNC(IApplicationDisplayService::GetSystemDisplayService)},
        {0x66, SFUNC(IApplicationDisplayService::GetManagerDisplayService)},
        {0x67, SFUNC(IApplicationDisplayService::GetIndirectDisplayTransactionService)},
        {0x3F2, SFUNC(IApplicationDisplayService::OpenDisplay)},
        {0x3FC, SFUNC(IApplicationDisplayService::CloseDisplay)},
        {0x7E4, SFUNC(IApplicationDisplayService::OpenLayer)},
        {0x7E5, SFUNC(IApplicationDisplayService::CloseLayer)},
        {0x835, SFUNC(IApplicationDisplayService::SetLayerScalingMode)},
        {0x1452, SFUNC(IApplicationDisplayService::GetDisplayVsyncEvent)},
    }) {}

    void IApplicationDisplayService::GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(nvnflinger::dispdrv), session, response);
    }

    void IApplicationDisplayService::GetIndirectDisplayTransactionService(skyline::kernel::type::KSession &session, skyline::kernel::ipc::IpcRequest &request, skyline::kernel::ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(nvnflinger::dispdrv), session, response);
    }

    void IApplicationDisplayService::GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISystemDisplayService), session, response);
    }

    void IApplicationDisplayService::GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IManagerDisplayService), session, response);
    }

    void IApplicationDisplayService::OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string displayName(reinterpret_cast<char *>(request.cmdArg));
        state.logger->Debug("Setting display as: {}", displayName);
        state.gpu->SetDisplay(displayName);
        response.Push<u64>(0); // There's only one display
    }

    void IApplicationDisplayService::CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Closing the display");
        state.gpu->CloseDisplay();
    }

    void IApplicationDisplayService::OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            char displayName[0x40];
            u64 layerId;
            u64 userId;
        } input = request.Pop<InputStruct>();
        state.logger->Debug("Opening Layer: Display Name: {}, Layer ID: {}, User ID: {}", input.displayName, input.layerId, input.userId);
        std::string name(input.displayName);
        gpu::Parcel parcel(state);
        LayerParcel data{
            .type = 0x20,
            .pid = 0,
            .bufferId = 0, // As we only have one layer and buffer
            .string = "dispdrv"
        };
        parcel.WriteData(data);
        parcel.objects.resize(4);
        response.Push(parcel.WriteParcel(request.outputBuf.at(0)));
    }

    void IApplicationDisplayService::CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u64 layerId = request.Pop<u64>();
        state.logger->Debug("Closing Layer: {}", layerId);
        if (state.gpu->layerStatus == gpu::LayerStatus::Uninitialized)
            state.logger->Warn("The application is destroying an uninitialized layer");
        state.gpu->layerStatus = gpu::LayerStatus::Uninitialized;
        response.Push<u32>(constant::status::Success);
    }

    void IApplicationDisplayService::SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto scalingMode = request.Pop<u64>();
        auto layerId = request.Pop<u64>();
        state.logger->Debug("Setting Layer Scaling mode to '{}' for layer {}", scalingMode, layerId);
    }

    void IApplicationDisplayService::GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        handle_t handle = state.process->InsertItem(state.gpu->vsyncEvent);
        state.logger->Debug("VSync Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
    }

    ISystemDisplayService::ISystemDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, Service::vi_ISystemDisplayService, "vi:ISystemDisplayService", {
        {0x89D, SFUNC(ISystemDisplayService::SetLayerZ)},
        {0x908, SFUNC(IDisplayService::CreateStrayLayer)}
    }) {}

    void ISystemDisplayService::SetLayerZ(skyline::kernel::type::KSession &session, skyline::kernel::ipc::IpcRequest &request, skyline::kernel::ipc::IpcResponse &response) {}

    IManagerDisplayService::IManagerDisplayService(const DeviceState &state, ServiceManager &manager) : IDisplayService(state, manager, Service::vi_IManagerDisplayService, "vi:IManagerDisplayService", {
        {0x7DA, SFUNC(IManagerDisplayService::CreateManagedLayer)},
        {0x7DB, SFUNC(IManagerDisplayService::DestroyManagedLayer)},
        {0x7DC, SFUNC(IDisplayService::CreateStrayLayer)},
        {0x1770, SFUNC(IManagerDisplayService::AddToLayerStack)}
    }) {}

    void IManagerDisplayService::CreateManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.Skip<u32>();
        u64 displayId = request.Pop<u64>();
        state.logger->Debug("Creating Managed Layer: {}", displayId);
        if (state.gpu->layerStatus == gpu::LayerStatus::Initialized)
            throw exception("The application is creating more than one layer");
        state.gpu->layerStatus = gpu::LayerStatus::Initialized;
        response.Push<u64>(0); // There's only one layer
    }

    void IManagerDisplayService::DestroyManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Destroying managed layer");
        if (state.gpu->layerStatus == gpu::LayerStatus::Uninitialized)
            state.logger->Warn("The application is destroying an uninitialized layer");
        state.gpu->layerStatus = gpu::LayerStatus::Uninitialized;
        response.Push<u32>(constant::status::Success);
    }

    void IManagerDisplayService::AddToLayerStack(skyline::kernel::type::KSession &session, skyline::kernel::ipc::IpcRequest &request, skyline::kernel::ipc::IpcResponse &response) {}
}
