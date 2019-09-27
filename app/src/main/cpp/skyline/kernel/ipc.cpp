#include <syslog.h>
#include <cstdlib>
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
            currPtr += sizeof(HandleDescriptor) + (handleDesc->send_pid ? sizeof(u8) : 0);
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
            vecBufX.push_back(reinterpret_cast<BufferDescriptorX *>(currPtr));
            currPtr += sizeof(BufferDescriptorX);
        }

        for (uint index = 0; header->a_no > index; index++) {
            vecBufA.push_back(reinterpret_cast<BufferDescriptorABW *>(currPtr));
            currPtr += sizeof(BufferDescriptorABW);
        }

        for (uint index = 0; header->b_no > index; index++) {
            vecBufB.push_back(reinterpret_cast<BufferDescriptorABW *>(currPtr));
            currPtr += sizeof(BufferDescriptorABW);
        }

        for (uint index = 0; header->w_no > index; index++) {
            vecBufW.push_back(reinterpret_cast<BufferDescriptorABW *>(currPtr));
            currPtr += sizeof(BufferDescriptorABW);
        }

        currPtr = reinterpret_cast<u8 *>((((reinterpret_cast<u64>(currPtr) - reinterpret_cast<u64>(tls.data())) - 1U) & ~(constant::PaddingSum - 1U)) + constant::PaddingSum + reinterpret_cast<u64>(tls.data())); // Align to 16 bytes relative to start of TLS

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
            cmdArgSz = (header->raw_sz * sizeof(u32)) - (constant::PaddingSum + sizeof(PayloadHeader));
            currPtr += cmdArgSz;
        }

        if (payload->magic != constant::SfciMagic)
            state.logger->Write(Logger::Debug, "Unexpected Magic in PayloadHeader: 0x{:X}", u32(payload->magic));

        if (header->c_flag == static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            vecBufC.push_back(reinterpret_cast<BufferDescriptorC *>(currPtr));
        } else if (header->c_flag > static_cast<u8>(BufferCFlag::SingleDescriptor)) {
            for (uint index = 0; (header->c_flag - 2) > index; index++) { // (c_flag - 2) C descriptors are present
                vecBufC.push_back(reinterpret_cast<BufferDescriptorC *>(currPtr));
                state.logger->Write(Logger::Debug, "Buf C #{} AD: 0x{:X} SZ: 0x{:X}", index, u64(vecBufC[index]->address), u16(vecBufC[index]->size));
                currPtr += sizeof(BufferDescriptorC);
            }
        }

        state.logger->Write(Logger::Debug, "Header: X No: {}, A No: {}, B No: {}, W No: {}, C No: {}, Raw Size: {}", u8(header->x_no), u8(header->a_no), u8(header->b_no), u8(header->w_no), u8(vecBufC.size()), u64(cmdArgSz));
        if (header->handle_desc)
            state.logger->Write(Logger::Debug, "Handle Descriptor: Send PID: {}, Copy Count: {}, Move Count: {}", bool(handleDesc->send_pid), u32(handleDesc->copy_count), u32(handleDesc->move_count));
        if (isDomain)
            state.logger->Write(Logger::Debug, "Domain Header: Command: {}, Input Object Count: {}, Object ID: 0x{:X}", domain->command, domain->input_count, domain->object_id);
        state.logger->Write(Logger::Debug, "Data Payload: Command ID: 0x{:X}", u32(payload->value));
    }

    IpcResponse::IpcResponse(bool isDomain, const DeviceState &state) : isDomain(isDomain), state(state) {}

    void IpcResponse::WriteTls() {
        std::array<u8, constant::TlsIpcSize> tls{};
        u8 *currPtr = tls.data();
        auto header = reinterpret_cast<CommandHeader *>(currPtr);
        header->raw_sz = static_cast<u32>((sizeof(PayloadHeader) + argVec.size() + (domainObjects.size() * sizeof(handle_t)) + constant::PaddingSum + (isDomain ? sizeof(DomainHeaderRequest) : 0)) / sizeof(u32)); // Size is in 32-bit units because Nintendo
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

        u64 padding = ((((reinterpret_cast<u64>(currPtr) - reinterpret_cast<u64>(tls.data())) - 1U) & ~(constant::PaddingSum - 1U)) + constant::PaddingSum + (reinterpret_cast<u64>(tls.data()) - reinterpret_cast<u64>(currPtr))); // Calculate the amount of padding at the front
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

        if(isDomain) {
            for (auto& domainObject : domainObjects) {
                *reinterpret_cast<handle_t *>(currPtr) = domainObject;
                currPtr += sizeof(handle_t);
            }
        }

        state.thisProcess->WriteMemory(tls.data(), state.thisThread->tls, constant::TlsIpcSize);
    }
}
