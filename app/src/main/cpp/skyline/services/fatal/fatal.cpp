#include "fatal.h"

namespace skyline::service::fatal {
    fatalU::fatalU(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::fatal_u, {
        {0x0, SFUNC(fatalU::ThrowFatal)},
        {0x1, SFUNC(fatalU::ThrowFatal)},
        {0x2, SFUNC(fatalU::ThrowFatal)}
    }) {}

    void fatalU::ThrowFatal(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        throw exception("A fatal error with code: 0x{:X} has caused emulation to stop", *reinterpret_cast<u32 *>(request.cmdArg));
    }
}
