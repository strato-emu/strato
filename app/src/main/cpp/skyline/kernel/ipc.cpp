#include "ipc.h"
#include "types/KProcess.h"

namespace skyline::kernel::ipc {
    IpcRequest::IpcRequest(bool isDomain, const DeviceState &state) : isDomain(isDomain), state(state), tls() {
        u8 *currPtr = tls.data();
        state.thisProcess->ReadMemory(currPtr, state.thisThread->tls, constant::TlsIpcSize);

        header = reinterpret_cast<CommandHeader *>(currPtr);
        currPtr += sizeof(CommandHeader);

        if (header->handle_desc) {
            handleDesc = reinterpret_cast<HandleDescriptor *>(currPtr);
            currPtr += sizeof(HandleDescriptor) + (handleDesc->send_pid ? sizeof(u64) : 0);
            for (uint index = 0; handleDesc->copy_count > index; index++) {
                copyHandles.push_back(*reinterpret_cast<handle_t *>(currPtr));
                currPtr += sizeof(handle_t);
            }
            for (uint index = 0; handleDesc->move_count > index; index++) {
                moveHandles.push_back(*reinterpret_cast<handle_t *>(currPtr));
                currPtr += sizeof(handle_t);
            }
        }

        for (uint index = 0; header->x_no > index; index++) {
            auto bufX = reinterpret_cast<BufferDescriptorX *>(currPtr);
            if (bufX->Address()) {
                vecBufX.push_back(bufX);
                state.logger->Debug("Buf X #{} AD: 0x{:X} SZ: 0x{:X} CTR: {}", index, u64(bufX->Address()), u16(bufX->size), u16(bufX->Counter()));
            }
            currPtr += sizeof(BufferDescriptorX);
        }

        for (uint index = 0; header->a_no > index; index++) {
            auto bufA = reinterpret_cast<BufferDescriptorABW *>(currPtr);
            if (bufA->Address()) {
                vecBufA.push_back(bufA);
                state.logger->Debug("Buf A #{} AD: 0x{:X} SZ: 0x{:X}", index, u64(bufA->Address()), u64(bufA->Size()));
            }
            currPtr += sizeof(BufferDescriptorABW);
        }

        for (uint index = 0; header->b_no > index; index++) {
            auto bufB = reinterpret_cast<BufferDescriptorABW *>(currPtr);
            if (bufB->Address()) {
                vecBufB.push_back(bufB);
                state.logger->Debug("Buf B #{} AD: 0x{:X} SZ: 0x{:X}", index, u64(bufB->Address()), u64(bufB->Size()));
            }
            currPtr += sizeof(BufferDescriptorABW);
        }

        for (uint index = 0; header->w_no > index; index++) {
            auto bufW = reinterpret_cast<BufferDescriptorABW *>(currPtr);
            if (bufW->Address()) {
                vecBufW.push_back(bufW);
                state.logger->Debug("Buf W #{} AD: 0x{:X} SZ: 0x{:X}", index, u64(bufW->Address()), u16(bufW->Size()));
            }
            currPtr += sizeof(BufferDescriptorABW);
        }

        u64 padding = ((((reinterpret_cast<u64>(currPtr) - reinterpret_cast<u64>(tls.data())) - 1U) & ~(constant::IpcPaddingSum - 1U)) + constant::IpcPaddingSum + (reinterpret_cast<u64>(tls.data()) - reinterpret_cast<u64>(currPtr))); // Calculate the amount of padding at the front
        currPtr += padding;

        if (isDomain) {
            domain = reinterpret_cast<DomainHeaderRequest *>(currPtr);
            currPtr += sizeof(DomainHeaderRequest);

            payload = reinterpret_cast<PayloadHeader *>(currPtr);
            currPtr += sizeof(PayloadHeader);

            cmdArg = currPtr;
            cmdArgSz = domain->payload_sz - sizeof(PayloadHeader);
            currPtr += domain->payload_sz;

            for (uint index = 0; domain->input_count > index; index++) {
                domainObjects.push_back(*reinterpret_cast<handle_t *>(currPtr));
                currPtr += sizeof(handle_t);
            }
        } else {
            payload = reinterpret_cast<PayloadHeader *>(currPtr);
            currPtr += sizeof(PayloadHeader);

            cmdArg = currPtr;
            cmdArgSz = (header->raw_sz * sizeof(u32)) - (constant::IpcPaddingSum + sizeof(PayloadHeader));
            currPtr += cmdArgSz;
        }

        if (payload->magic != constant::SfciMagic && header->type != static_cast<u16>(CommandType::Control))
            state.logger->Debug("Unexpected Magic in PayloadHeader: 0x{:X}", u32(payload->magic));

        currPtr += constant::IpcPaddingSum - padding;

        if (header->c_flag == static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            auto bufC = reinterpret_cast<BufferDescriptorC *>(currPtr);
            vecBufC.push_back(bufC);
            state.logger->Debug("Buf C: AD: 0x{:X} SZ: 0x{:X}", u64(bufC->address), u16(bufC->size));
        } else if (header->c_flag > static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            for (uint index = 0; (header->c_flag - 2) > index; index++) { // (c_flag - 2) C descriptors are present
                auto bufC = reinterpret_cast<BufferDescriptorC *>(currPtr);
                if (bufC->address) {
                    vecBufC.push_back(bufC);
                    state.logger->Debug("Buf C #{} AD: 0x{:X} SZ: 0x{:X}", index, u64(bufC->address), u16(bufC->size));
                }
                currPtr += sizeof(BufferDescriptorC);
            }
        }

        state.logger->Debug("Header: X No: {}, A No: {}, B No: {}, W No: {}, C No: {}, Raw Size: {}", u8(header->x_no), u8(header->a_no), u8(header->b_no), u8(header->w_no), u8(vecBufC.size()), u64(cmdArgSz));
        if (header->handle_desc)
            state.logger->Debug("Handle Descriptor: Send PID: {}, Copy Count: {}, Move Count: {}", bool(handleDesc->send_pid), u32(handleDesc->copy_count), u32(handleDesc->move_count));
        if (isDomain)
            state.logger->Debug("Domain Header: Command: {}, Input Object Count: {}, Object ID: 0x{:X}", domain->command, domain->input_count, domain->object_id);
        state.logger->Debug("Data Payload: Command ID: 0x{:X}", u32(payload->value));
    }

    IpcResponse::IpcResponse(bool isDomain, const DeviceState &state) : isDomain(isDomain), state(state) {}

    void IpcResponse::WriteTls() {
        std::array<u8, constant::TlsIpcSize> tls{};
        u8 *currPtr = tls.data();
        auto header = reinterpret_cast<CommandHeader *>(currPtr);
        header->raw_sz = static_cast<u32>((sizeof(PayloadHeader) + argVec.size() + (domainObjects.size() * sizeof(handle_t)) + constant::IpcPaddingSum + (isDomain ? sizeof(DomainHeaderRequest) : 0)) / sizeof(u32)); // Size is in 32-bit units because Nintendo
        header->handle_desc = (!copyHandles.empty() || !moveHandles.empty());
        currPtr += sizeof(CommandHeader);

        if (header->handle_desc) {
            auto handleDesc = reinterpret_cast<HandleDescriptor *>(currPtr);
            handleDesc->copy_count = static_cast<u8>(copyHandles.size());
            handleDesc->move_count = static_cast<u8>(moveHandles.size());
            currPtr += sizeof(HandleDescriptor);

            for (unsigned int copyHandle : copyHandles) {
                *reinterpret_cast<handle_t *>(currPtr) = copyHandle;
                currPtr += sizeof(handle_t);
            }

            for (unsigned int moveHandle : moveHandles) {
                *reinterpret_cast<handle_t *>(currPtr) = moveHandle;
                currPtr += sizeof(handle_t);
            }
        }

        u64 padding = ((((reinterpret_cast<u64>(currPtr) - reinterpret_cast<u64>(tls.data())) - 1U) & ~(constant::IpcPaddingSum - 1U)) + constant::IpcPaddingSum + (reinterpret_cast<u64>(tls.data()) - reinterpret_cast<u64>(currPtr))); // Calculate the amount of padding at the front
        currPtr += padding;

        if (isDomain) {
            auto domain = reinterpret_cast<DomainHeaderResponse *>(currPtr);
            domain->output_count = static_cast<u32>(domainObjects.size());
            currPtr += sizeof(DomainHeaderResponse);
        }

        auto payload = reinterpret_cast<PayloadHeader *>(currPtr);
        payload->magic = constant::SfcoMagic;
        payload->version = 1;
        payload->value = errorCode;
        currPtr += sizeof(PayloadHeader);
        if (!argVec.empty())
            memcpy(currPtr, argVec.data(), argVec.size());
        currPtr += argVec.size();

        if (isDomain) {
            for (auto &domainObject : domainObjects) {
                *reinterpret_cast<handle_t *>(currPtr) = domainObject;
                currPtr += sizeof(handle_t);
            }
        }

        state.logger->Debug("Output: Raw Size: {}, Command ID: 0x{:X}, Copy Handles: {}, Move Handles: {}", u32(header->raw_sz), u32(payload->value), copyHandles.size(), moveHandles.size());

        state.thisProcess->WriteMemory(tls.data(), state.thisThread->tls, constant::TlsIpcSize);
    }

    std::vector<u8> BufferDescriptorABW::Read(const DeviceState &state) {
        std::vector<u8> vec(Size());
        state.thisProcess->ReadMemory(vec.data(), Address(), Size());
        return std::move(vec);
    }
}
