#include "sm.h"

namespace skyline::kernel::service::sm {
    sm::sm(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::sm, {
        {0x0, SFunc(sm::Initialize)},
        {0x1, SFunc(sm::GetService)}
    }) {}

    void sm::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void sm::GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        std::string serviceName(reinterpret_cast<char *>(request.cmdArg));
        if (serviceName.empty())
            response.errorCode = constant::status::ServiceInvName;
        else {
            try {
                manager.NewService(ServiceString.at(serviceName), session, response);
                state.logger->Write(Logger::Debug, "Service has been registered: \"{}\"", serviceName);
            } catch (std::out_of_range &) {
                response.errorCode = constant::status::ServiceNotReg;
                state.logger->Write(Logger::Error, "Service has not been implemented: \"{}\"", serviceName);
            }
        }
    }
}
