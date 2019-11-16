#include "sm.h"

namespace skyline::service::sm {
    sm::sm(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::sm, {
        {0x0, SFUNC(sm::Initialize)},
        {0x1, SFUNC(sm::GetService)}
    }) {}

    void sm::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void sm::GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string serviceName(reinterpret_cast<char *>(request.cmdArg));
        if (serviceName.empty())
            response.errorCode = constant::status::ServiceInvName;
        else {
            try {
                manager.NewService(serviceName, session, response);
            } catch (std::out_of_range &) {
                response.errorCode = constant::status::ServiceNotReg;
                state.logger->Warn("Service has not been implemented: \"{}\"", serviceName);
            }
        }
    }
}
