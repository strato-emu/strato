#include "hid.h"
#include <os.h>

namespace skyline::kernel::service::hid {
    IAppletResource::IAppletResource(const DeviceState &state, ServiceManager& manager) : BaseService(state, manager, false, Service::hid_IAppletResource, {
        {0x0, SFunc(IAppletResource::GetSharedMemoryHandle)}
    }) {}

    void IAppletResource::GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        hidSharedMemory = state.os->MapSharedKernel(0, constant::hidSharedMemSize, memory::Permission(true, false, false), memory::Permission(true, true, false), memory::Type::SharedMemory);
        response.copyHandles.push_back(state.thisProcess->InsertItem<type::KSharedMemory>(hidSharedMemory));
    }

    hid::hid(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::hid, {
        {0x0, SFunc(hid::CreateAppletResource)},
        {0x64, SFunc(hid::SetSupportedNpadStyleSet)},
        {0x66, SFunc(hid::SetSupportedNpadIdType)},
        {0x67, SFunc(hid::ActivateNpad)},
        {0x78, SFunc(hid::SetNpadJoyHoldType)},
        {0x7A, SFunc(hid::SetNpadJoyAssignmentModeSingleByDefault)},
        {0x7B, SFunc(hid::SetNpadJoyAssignmentModeSingle)},
        {0x7C, SFunc(hid::SetNpadJoyAssignmentModeDual)}
    }) {}

    void hid::CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        resource = std::make_shared<IAppletResource>(state, manager);
        manager.RegisterService(resource, session, response);
    }

    void hid::SetSupportedNpadStyleSet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u32 styleSet;
            u64 appletUserId;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        styleSet = *reinterpret_cast<StyleSet *>(&input->styleSet);
        state.logger->Write(Logger::Debug, "Controller Support:   Pro-Controller: {}   Joy-Con: Handheld: {}, Dual: {}, L: {}, R: {}   GameCube: {}   PokeBall: {}   NES: {}   NES Handheld: {}   SNES: {}", static_cast<bool>(styleSet->pro_controller), static_cast<bool>(styleSet->joycon_handheld), static_cast<bool>(styleSet->joycon_dual), static_cast<bool>(styleSet->joycon_left), static_cast<bool>
        (styleSet->joycon_right), static_cast<bool>(styleSet->gamecube), static_cast<bool>(styleSet->pokeball), static_cast<bool>(styleSet->nes), static_cast<bool>(styleSet->nes_handheld), static_cast<bool>(styleSet->snes));
    }

    void hid::SetSupportedNpadIdType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto &buffer = request.vecBufX[0];
        uint numId = buffer->size / sizeof(NpadId);
        u64 address = buffer->Address();
        for (uint i = 0; i < numId; i++) {
            auto id = state.thisProcess->ReadMemory<NpadId>(address);
            deviceMap[id] = JoyConDevice(id);
            address += sizeof(NpadId);
        }
    }

    void hid::ActivateNpad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {}

    void hid::SetNpadJoyHoldType(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            NpadId controllerId;
            u64 appletUserId;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        deviceMap[input->controllerId].assignment = JoyConAssignment::Single;
    }

    void hid::SetNpadJoyAssignmentModeSingleByDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            u64 appletUserId;
            JoyConOrientation orientation;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        orientation = input->orientation;
    }

    void hid::SetNpadJoyAssignmentModeSingle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            NpadId controllerId;
            u64 appletUserId;
            JoyConSide joyDeviceType;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        deviceMap[input->controllerId].assignment = JoyConAssignment::Single;
        deviceMap[input->controllerId].side = input->joyDeviceType;
    }

    void hid::SetNpadJoyAssignmentModeDual(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        struct InputStruct {
            NpadId controllerType;
            u64 appletUserId;
        } *input = reinterpret_cast<InputStruct *>(request.cmdArg);
        deviceMap[input->controllerType].assignment = JoyConAssignment::Dual;
    }
}
