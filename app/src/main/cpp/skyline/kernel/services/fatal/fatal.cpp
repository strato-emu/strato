#include "fatal.h"

namespace skyline::kernel::service::fatal {
    fatalU::fatalU(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::fatal_u, {
        {0x0, SFunc(fatalU::ThrowFatal)}
    }) {}

    void fatalU::ThrowFatal(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        throw exception("A fatal error has caused emulation to stop");
    }
}
