#include "fs.h"

namespace skyline::service::fs {
    fsp::fsp(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::fs_fsp, "fs:fsp", {
        {0x1, SFUNC(fsp::SetCurrentProcess)},
        {0x12, SFUNC(fsp::OpenSdCardFileSystem)}
    }) {}

    void fsp::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = *reinterpret_cast<pid_t *>(request.cmdArg);
    }

    void fsp::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(FsType::SdCard, state, manager), session, response);
    }

    IFileSystem::IFileSystem(FsType type, const DeviceState &state, ServiceManager &manager) : type(type), BaseService(state, manager, Service::fs_IFileSystem, "fs:IFileSystem", {}) {}
}
