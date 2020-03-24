#include <os.h>
#include <services/hosbinder/IHOSBinderDriver.h>
#include <services/hosbinder/display.h>
#include "ISelfController.h"

namespace skyline::service::am {
    ISelfController::ISelfController(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::am_ISelfController, "am:ISelfController", {
        {0xB, SFUNC(ISelfController::SetOperationModeChangedNotification)},
        {0xC, SFUNC(ISelfController::SetPerformanceModeChangedNotification)},
        {0xD, SFUNC(ISelfController::SetFocusHandlingMode)},
        {0x10, SFUNC(ISelfController::SetOutOfFocusSuspendingEnabled)},
        {0x28, SFUNC(ISelfController::CreateManagedDisplayLayer)}
    }) {}

    void ISelfController::SetOperationModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetPerformanceModeChangedNotification(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetFocusHandlingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::SetOutOfFocusSuspendingEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void ISelfController::CreateManagedDisplayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        state.logger->Debug("Creating Managed Layer on Default Display");
        auto hosBinder = state.os->serviceManager.GetService<hosbinder::IHOSBinderDriver>(Service::hosbinder_IHOSBinderDriver);
        if (hosBinder->layerStatus != hosbinder::LayerStatus::Uninitialized)
            throw exception("The application is creating more than one layer");
        hosBinder->layerStatus = hosbinder::LayerStatus::Managed;
        response.Push<u64>(0);
    }
}
