#include "hid.h"
#include <os.h>

namespace skyline::kernel::service::hid {
    hid::hid(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::hid, {
        {0x0, SFunc(hid::CreateAppletResource)}
    }) {}

    void hid::CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        resource = std::static_pointer_cast<IAppletResource>(manager.NewService(Service::hid_IAppletResource, session, response));
    }

    IAppletResource::IAppletResource(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::hid_IAppletResource, {
        {0x0, SFunc(IAppletResource::GetSharedMemoryHandle)}
    }) {}

    void IAppletResource::GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        hidSharedMemory = state.os->MapSharedKernel(0, constant::hidSharedMemSize, memory::Permission(true, false, false), memory::Permission(true, true, false), memory::Type::SharedMemory);
        response.copyHandles.push_back(state.thisProcess->InsertItem<type::KSharedMemory>(hidSharedMemory));
    }
}
