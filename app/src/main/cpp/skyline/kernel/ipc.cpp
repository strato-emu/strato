// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ipc.h"
#include "types/KProcess.h"

namespace skyline::kernel::ipc {
    IpcRequest::IpcRequest(bool isDomain, const DeviceState &state) : isDomain(isDomain) {
        auto tls{state.ctx->tpidrroEl0};
        u8 *pointer{tls};

        header = reinterpret_cast<CommandHeader *>(pointer);
        pointer += sizeof(CommandHeader);

        size_t cBufferLengthSize{util::AlignUp(((header->cFlag == BufferCFlag::None) ? 0 : ((header->cFlag > BufferCFlag::SingleDescriptor) ? (static_cast<u8>(header->cFlag) - 2) : 1)) * sizeof(u16), sizeof(u32))};

        if (header->handleDesc) {
            handleDesc = reinterpret_cast<HandleDescriptor *>(pointer);
            pointer += sizeof(HandleDescriptor) + (handleDesc->sendPid ? sizeof(u64) : 0);
            for (u32 index{}; handleDesc->copyCount > index; index++) {
                copyHandles.push_back(*reinterpret_cast<KHandle *>(pointer));
                pointer += sizeof(KHandle);
            }
            for (u32 index{}; handleDesc->moveCount > index; index++) {
                moveHandles.push_back(*reinterpret_cast<KHandle *>(pointer));
                pointer += sizeof(KHandle);
            }
        }

        for (u8 index{}; header->xNo > index; index++) {
            auto bufX{reinterpret_cast<BufferDescriptorX *>(pointer)};
            if (bufX->Pointer()) {
                inputBuf.emplace_back(bufX->Pointer(), static_cast<u16>(bufX->size));
                state.logger->Debug("Buf X #{}: 0x{:X}, 0x{:X}, #{}", index, bufX->Pointer(), static_cast<u16>(bufX->size), static_cast<u16>(bufX->Counter()));
            }
            pointer += sizeof(BufferDescriptorX);
        }

        for (u8 index{}; header->aNo > index; index++) {
            auto bufA{reinterpret_cast<BufferDescriptorABW *>(pointer)};
            if (bufA->Pointer()) {
                inputBuf.emplace_back(bufA->Pointer(), bufA->Size());
                state.logger->Debug("Buf A #{}: 0x{:X}, 0x{:X}", index, bufA->Pointer(), static_cast<u64>(bufA->Size()));
            }
            pointer += sizeof(BufferDescriptorABW);
        }

        for (u8 index{}; header->bNo > index; index++) {
            auto bufB{reinterpret_cast<BufferDescriptorABW *>(pointer)};
            if (bufB->Pointer()) {
                outputBuf.emplace_back(bufB->Pointer(), bufB->Size());
                state.logger->Debug("Buf B #{}: 0x{:X}, 0x{:X}", index, bufB->Pointer(), static_cast<u64>(bufB->Size()));
            }
            pointer += sizeof(BufferDescriptorABW);
        }

        for (u8 index{}; header->wNo > index; index++) {
            auto bufW{reinterpret_cast<BufferDescriptorABW *>(pointer)};
            if (bufW->Pointer()) {
                outputBuf.emplace_back(bufW->Pointer(), bufW->Size());
                outputBuf.emplace_back(bufW->Pointer(), bufW->Size());
                state.logger->Debug("Buf W #{}: 0x{:X}, 0x{:X}", index, bufW->Pointer(), static_cast<u16>(bufW->Size()));
            }
            pointer += sizeof(BufferDescriptorABW);
        }

        auto offset{pointer - tls}; // We calculate the relative offset as the absolute one might differ
        auto padding{util::AlignUp(offset, constant::IpcPaddingSum) - offset}; // Calculate the amount of padding at the front
        pointer += padding;

        if (isDomain && (header->type == CommandType::Request || header->type == CommandType::RequestWithContext)) {
            domain = reinterpret_cast<DomainHeaderRequest *>(pointer);
            pointer += sizeof(DomainHeaderRequest);

            payload = reinterpret_cast<PayloadHeader *>(pointer);
            pointer += sizeof(PayloadHeader);

            cmdArg = pointer;
            cmdArgSz = domain->payloadSz - sizeof(PayloadHeader);
            pointer += cmdArgSz;

            for (u8 index{}; domain->inputCount > index; index++) {
                domainObjects.push_back(*reinterpret_cast<KHandle *>(pointer));
                pointer += sizeof(KHandle);
            }
        } else {
            payload = reinterpret_cast<PayloadHeader *>(pointer);
            pointer += sizeof(PayloadHeader);

            cmdArg = pointer;
            cmdArgSz = (header->rawSize * sizeof(u32)) - (constant::IpcPaddingSum + sizeof(PayloadHeader)) - cBufferLengthSize;
            pointer += cmdArgSz;
        }

        payloadOffset = cmdArg;

        if (payload->magic != util::MakeMagic<u32>("SFCI") && (header->type != CommandType::Control && header->type != CommandType::ControlWithContext)) // SFCI is the magic in received IPC messages
            state.logger->Debug("Unexpected Magic in PayloadHeader: 0x{:X}", static_cast<u32>(payload->magic));

        pointer += constant::IpcPaddingSum - padding + cBufferLengthSize;

        if (header->cFlag == BufferCFlag::SingleDescriptor) {
            auto bufC{reinterpret_cast<BufferDescriptorC *>(pointer)};
            if (bufC->address) {
                outputBuf.emplace_back(bufC->Pointer(), static_cast<u16>(bufC->size));
                state.logger->Debug("Buf C: 0x{:X}, 0x{:X}", bufC->Pointer(), static_cast<u16>(bufC->size));
            }
        } else if (header->cFlag > BufferCFlag::SingleDescriptor) {
            for (u8 index{}; (static_cast<u8>(header->cFlag) - 2) > index; index++) { // (cFlag - 2) C descriptors are present
                auto bufC{reinterpret_cast<BufferDescriptorC *>(pointer)};
                if (bufC->address) {
                    outputBuf.emplace_back(bufC->Pointer(), static_cast<u16>(bufC->size));
                    state.logger->Debug("Buf C #{}: 0x{:X}, 0x{:X}", index, bufC->Pointer(), static_cast<u16>(bufC->size));
                }
                pointer += sizeof(BufferDescriptorC);
            }
        }

        if (header->type == CommandType::Request || header->type == CommandType::RequestWithContext) {
            state.logger->Debug("Header: Input No: {}, Output No: {}, Raw Size: {}", inputBuf.size(), outputBuf.size(), static_cast<u64>(cmdArgSz));
            if (header->handleDesc)
                state.logger->Debug("Handle Descriptor: Send PID: {}, Copy Count: {}, Move Count: {}", static_cast<bool>(handleDesc->sendPid), static_cast<u32>(handleDesc->copyCount), static_cast<u32>(handleDesc->moveCount));
            if (isDomain)
                state.logger->Debug("Domain Header: Command: {}, Input Object Count: {}, Object ID: 0x{:X}", domain->command, domain->inputCount, domain->objectId);
            state.logger->Debug("Command ID: 0x{:X}", static_cast<u32>(payload->value));
        }
    }

    IpcResponse::IpcResponse(const DeviceState &state) : state(state) {}

    void IpcResponse::WriteResponse(bool isDomain) {
        auto tls{state.ctx->tpidrroEl0};
        u8 *pointer{tls};

        memset(tls, 0, constant::TlsIpcSize);

        auto header{reinterpret_cast<CommandHeader *>(pointer)};
        header->rawSize = static_cast<u32>((sizeof(PayloadHeader) + payload.size() + (domainObjects.size() * sizeof(KHandle)) + constant::IpcPaddingSum + (isDomain ? sizeof(DomainHeaderRequest) : 0)) / sizeof(u32)); // Size is in 32-bit units because Nintendo
        header->handleDesc = (!copyHandles.empty() || !moveHandles.empty());
        pointer += sizeof(CommandHeader);

        if (header->handleDesc) {
            auto handleDesc{reinterpret_cast<HandleDescriptor *>(pointer)};
            handleDesc->copyCount = static_cast<u8>(copyHandles.size());
            handleDesc->moveCount = static_cast<u8>(moveHandles.size());
            pointer += sizeof(HandleDescriptor);

            for (auto copyHandle : copyHandles) {
                *reinterpret_cast<KHandle *>(pointer) = copyHandle;
                pointer += sizeof(KHandle);
            }

            for (auto moveHandle : moveHandles) {
                *reinterpret_cast<KHandle *>(pointer) = moveHandle;
                pointer += sizeof(KHandle);
            }
        }

        auto offset{pointer - tls}; // We calculate the relative offset as the absolute one might differ
        auto padding{util::AlignUp(offset, constant::IpcPaddingSum) - offset}; // Calculate the amount of padding at the front
        pointer += padding;

        if (isDomain) {
            auto domain{reinterpret_cast<DomainHeaderResponse *>(pointer)};
            domain->outputCount = static_cast<u32>(domainObjects.size());
            pointer += sizeof(DomainHeaderResponse);
        }

        auto payloadHeader{reinterpret_cast<PayloadHeader *>(pointer)};
        payloadHeader->magic = util::MakeMagic<u32>("SFCO"); // SFCO is the magic in IPC responses
        payloadHeader->version = 1;
        payloadHeader->value = errorCode;
        pointer += sizeof(PayloadHeader);

        if (!payload.empty())
            std::memcpy(pointer, payload.data(), payload.size());
        pointer += payload.size();

        if (isDomain) {
            for (auto &domainObject : domainObjects) {
                *reinterpret_cast<KHandle *>(pointer) = domainObject;
                pointer += sizeof(KHandle);
            }
        }

        state.logger->Debug("Output: Raw Size: {}, Command ID: 0x{:X}, Copy Handles: {}, Move Handles: {}", static_cast<u32>(header->rawSize), static_cast<u32>(payloadHeader->value), copyHandles.size(), moveHandles.size());
    }
}
