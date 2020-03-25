#include "IAppletResource.h"

namespace skyline::service::hid {
    IAppletResource::IAppletResource(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::hid_IAppletResource, "hid:IAppletResource", {
        {0x0, SFUNC(IAppletResource::GetSharedMemoryHandle)}
    }) {}

    void IAppletResource::GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        hidSharedMemory = std::make_shared<kernel::type::KSharedMemory>(state, NULL, constant::HidSharedMemSize, memory::Permission{true, false, false});
        auto handle = state.process->InsertItem<type::KSharedMemory>(hidSharedMemory);
        state.logger->Debug("HID Shared Memory Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
    }
}
