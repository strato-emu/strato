// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc/gm20b/gmmu.h>
#include "inline2memory.h"

namespace skyline::soc::gm20b::engine {
    Inline2MemoryBackend::Inline2MemoryBackend(std::shared_ptr<AddressSpaceContext> addressSpaceContext) : addressSpaceContext(std::move(addressSpaceContext)) {}

    void Inline2MemoryBackend::LaunchDma(Inline2MemoryBackend::RegisterState &state) {
        writeOffset = 0;
        size_t targetSizeWords{(state.lineCount * util::AlignUp(state.lineLengthIn, 4)) / 4};
        buffer.resize(targetSizeWords);
    }

    void Inline2MemoryBackend::CompleteDma(Inline2MemoryBackend::RegisterState &state) {
        if (state.launchDma.completion == RegisterState::DmaCompletionType::ReleaseSemaphore)
            throw exception("Semaphore release on I2M completion is not supported!");

        if (state.launchDma.layout == RegisterState::DmaDstMemoryLayout::Pitch && state.lineCount == 1) {
            // TODO: we can do this with the buffer manager to avoid some overhead in the future
            Logger::Debug("range: 0x{:X} -> 0x{:X}", u64{state.offsetOut}, u64{state.offsetOut} + buffer.size() * 0x4);
            addressSpaceContext->gmmu.Write(state.offsetOut, buffer);
        } else {
            Logger::Warn("Non-linear I2M uploads are not supported!");
        }
    }

    void Inline2MemoryBackend::LoadInlineData(RegisterState &state, u32 value) {
        if (writeOffset >= buffer.size())
            throw exception("Inline data load overflow!");

        buffer[writeOffset++] = value;

        if (writeOffset == buffer.size())
            CompleteDma(state);
    }

    void Inline2MemoryBackend::LoadInlineData(Inline2MemoryBackend::RegisterState &state, span<u32> data) {
        if (writeOffset + data.size() > buffer.size())
            throw exception("Inline data load overflow!");

        span(buffer).subspan(writeOffset).copy_from(data);
        writeOffset += data.size();

        if (writeOffset == buffer.size())
            CompleteDma(state);
    }

    Inline2Memory::Inline2Memory(std::shared_ptr<AddressSpaceContext> addressSpaceContext) : backend(std::move(addressSpaceContext)) {}

    __attribute__((always_inline)) void Inline2Memory::CallMethod(u32 method, u32 argument) {
        Logger::Verbose("Called method in I2M: 0x{:X} args: 0x{:X}", method, argument);

        HandleMethod(method, argument);
    }

#define INLINE2MEMORY_OFFSET(field) (sizeof(typeof(Registers::field)) - sizeof(std::remove_reference_t<decltype(*Registers::field)>)) / sizeof(u32)
#define INLINE2MEMORY_STRUCT_OFFSET(field, member) INLINE2MEMORY_OFFSET(field) + U32_OFFSET(std::remove_reference_t<decltype(*Registers::field)>, member)

    void Inline2Memory::HandleMethod(u32 method, u32 argument) {
        registers.raw[method] = argument;

        switch (method) {
            case INLINE2MEMORY_STRUCT_OFFSET(i2m, launchDma):
                backend.LaunchDma(*registers.i2m);
                return;
            case INLINE2MEMORY_STRUCT_OFFSET(i2m, loadInlineData):
                backend.LoadInlineData(*registers.i2m, argument);
                return;
            default:
                return;
        }

    }

    void Inline2Memory::CallMethodBatchNonInc(u32 method, span<u32> arguments) {
        switch (method) {
            case INLINE2MEMORY_STRUCT_OFFSET(i2m, loadInlineData):
                backend.LoadInlineData(*registers.i2m, arguments);
                return;
            default:
                break;
        }

        for (u32 argument : arguments)
            HandleMethod(method, argument);
    }

#undef INLINE2MEMORY_STRUCT_OFFSET
#undef INLINE2MEMORY_OFFSET
}
