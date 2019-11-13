#include "nvdrv.h"
#include <kernel/types/KProcess.h>

namespace skyline::kernel::service::nvdrv {
    nvdrv::nvdrv(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::nvdrv, {
        {0x0, SFUNC(nvdrv::Open)},
        {0x1, SFUNC(nvdrv::Ioctl)},
        {0x2, SFUNC(nvdrv::Close)},
        {0x3, SFUNC(nvdrv::Initialize)},
        {0x4, SFUNC(nvdrv::QueryEvent)},
        {0x8, SFUNC(nvdrv::SetAruidByPID)}
    }) {}

    void nvdrv::Open(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto aBuf = request.vecBufA[0];
        std::string path(aBuf->Size(), '\0');
        state.thisProcess->ReadMemory(path.data(), aBuf->Address(), aBuf->Size());
        response.WriteValue<u32>(state.gpu->OpenDevice(path));
        response.WriteValue<u32>(constant::status::Success);
    }

    void nvdrv::Ioctl(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u32 fd;
            u32 cmd;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        state.gpu->Ioctl(input->fd, input->cmd, request, response);
    }

    void nvdrv::Close(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u32 fd = *reinterpret_cast<u32 *>(request.cmdArg);
        state.gpu->CloseDevice(fd);
        response.WriteValue<u32>(constant::status::Success);
    }

    void nvdrv::Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue<u32>(constant::status::Success);
    }

    void nvdrv::QueryEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u32 fd;
            u32 eventId;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        state.logger->Info("QueryEvent: FD: {}, Event ID: {}", input->fd, input->eventId);
        auto event = std::make_shared<type::KEvent>(state);
        response.copyHandles.push_back(state.thisProcess->InsertItem<type::KEvent>(event));
    }

    void nvdrv::SetAruidByPID(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.WriteValue<u32>(constant::status::Success);
    }
}
