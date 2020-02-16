#include "nvdrv.h"
#include <kernel/types/KProcess.h>

namespace skyline::service::nvdrv {
    nvdrv::nvdrv(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::nvdrv, "nvdrv", {
        {0x0, SFUNC(nvdrv::Open)},
        {0x1, SFUNC(nvdrv::Ioctl)},
        {0x2, SFUNC(nvdrv::Close)},
        {0x3, SFUNC(nvdrv::Initialize)},
        {0x4, SFUNC(nvdrv::QueryEvent)},
        {0x8, SFUNC(nvdrv::SetAruidByPID)}
    }) {}

    void nvdrv::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto buffer = request.inputBuf.at(0);
        auto path = state.process->GetString(buffer.address, buffer.size);
        response.Push<u32>(state.gpu->OpenDevice(path));
        response.Push<u32>(constant::status::Success);
    }

    void nvdrv::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto cmd = request.Pop<u32>();
        state.gpu->Ioctl(fd, cmd, request, response);
    }

    void nvdrv::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 fd = *reinterpret_cast<u32 *>(request.cmdArg);
        state.gpu->CloseDevice(fd);
        response.Push<u32>(constant::status::Success);
    }

    void nvdrv::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(constant::status::Success);
    }

    void nvdrv::QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto fd = request.Pop<u32>();
        auto eventId = request.Pop<u32>();
        auto event = std::make_shared<type::KEvent>(state);
        auto handle = state.process->InsertItem<type::KEvent>(event);
        state.logger->Debug("QueryEvent: FD: {}, Event ID: {}, Handle: {}", fd, eventId, handle);
        response.copyHandles.push_back(handle);
    }

    void nvdrv::SetAruidByPID(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(constant::status::Success);
    }
}
