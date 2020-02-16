#include "apm.h"

namespace skyline::service::apm {
    apm::apm(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::apm, "apm", {
        {0x0, SFUNC(apm::OpenSession)}
    }) {}

    void apm::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISession>(state, manager), session, response);
    }

    ISession::ISession(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::apm_ISession, "apm:ISession", {
        {0x0, SFUNC(ISession::SetPerformanceConfiguration)},
        {0x1, SFUNC(ISession::GetPerformanceConfiguration)}
    }) {}

    void ISession::SetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto mode = request.Pop<u32>();
        auto config = request.Pop<u32>();
        performanceConfig[mode] = config;
        state.logger->Info("SetPerformanceConfiguration called with 0x{:X} ({})", config, mode ? "Docked" : "Handheld");
    }

    void ISession::GetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 performanceMode = request.Pop<u32>();
        response.Push<u32>(performanceConfig[performanceMode]);
    }
}
