#include "apm.h"

namespace skyline::kernel::service::apm {
    apm::apm(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::apm, {
        {0x0, SFunc(apm::OpenSession)}
    }) {}

    void apm::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.NewService(Service::apm_ISession, session, response);
    }

    ISession::ISession(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::apm_ISession, {
        {0x0, SFunc(ISession::SetPerformanceConfiguration)},
        {0x1, SFunc(ISession::GetPerformanceConfiguration)}
    }) {}

    void ISession::SetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u32 mode;
            u32 config;
        } *performance = reinterpret_cast<InputStruct *>(request.cmdArg);
        performanceConfig[performance->mode] = performance->config;
        state.logger->Write(Logger::Info, "SetPerformanceConfiguration called with 0x{:X} ({})", performance->config, performance->mode ? "Docked" : "Handheld");
    }

    void ISession::GetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 performanceMode = *reinterpret_cast<u32 *>(request.cmdArg);
        response.WriteValue<u32>(performanceConfig[performanceMode]);
    }
}
