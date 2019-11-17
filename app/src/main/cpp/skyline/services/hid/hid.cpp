#include "hid.h"
#include <os.h>

namespace skyline::service::hid {
    IAppletResource::IAppletResource(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::hid_IAppletResource, {
        {0x0, SFUNC(IAppletResource::GetSharedMemoryHandle)}
    }) {}

    void IAppletResource::GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        hidSharedMemory = std::make_shared<kernel::type::KSharedMemory>(state, NULL, constant::hidSharedMemSize, memory::Permission(true, false, false), memory::Type::SharedMemory);
        auto handle = state.thisProcess->InsertItem<type::KSharedMemory>(hidSharedMemory);
        state.logger->Debug("HID Shared Memory Handle: {}", handle);
        response.copyHandles.push_back(handle);
    }

    hid::hid(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::hid, {
        {0x0, SFUNC(hid::CreateAppletResource)},
        {0x64, SFUNC(hid::SetSupportedNpadStyleSet)},
        {0x66, SFUNC(hid::SetSupportedNpadIdType)},
        {0x67, SFUNC(hid::ActivateNpad)},
        {0x78, SFUNC(hid::SetNpadJoyHoldType)},
        {0x7A, SFUNC(hid::SetNpadJoyAssignmentModeSingleByDefault)},
        {0x7B, SFUNC(hid::SetNpadJoyAssignmentModeSingle)},
        {0x7C, SFUNC(hid::SetNpadJoyAssignmentModeDual)}
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
        state.logger->Debug("Controller Support:\nPro-Controller: {}\nJoy-Con: Handheld: {}, Dual: {}, L: {}, R: {}\nGameCube: {}\nPokeBall: {}\nNES: {}, NES Handheld: {}, SNES: {}", static_cast<bool>(styleSet->proController), static_cast<bool>(styleSet->joyconHandheld), static_cast<bool>(styleSet->joyconDual), static_cast<bool>(styleSet->joyconLeft), static_cast<bool>
        (styleSet->joyconRight), static_cast<bool>(styleSet->gamecube), static_cast<bool>(styleSet->pokeball), static_cast<bool>(styleSet->nes), static_cast<bool>(styleSet->nesHandheld), static_cast<bool>(styleSet->snes));
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
