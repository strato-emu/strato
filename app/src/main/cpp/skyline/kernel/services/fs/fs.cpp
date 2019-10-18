#include "fs.h"

namespace skyline::kernel::service::fs {
    fsp::fsp(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::fs_fsp, {
        {0x1, SFunc(fsp::SetCurrentProcess)},
        {0x12, SFunc(fsp::OpenSdCardFileSystem)}
    }) {}

    void fsp::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = *reinterpret_cast<pid_t*>(request.cmdArg);
    }

    void fsp::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(FsType::SdCard, state, manager), session, response);
    }

    IFileSystem::IFileSystem(FsType type, const DeviceState &state, ServiceManager &manager) : type(type), BaseService(state, manager, false, Service::fs_IFileSystem, {}) {}
}
