// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/sharedmem.h>
#include <asm/unistd.h>
#include <unistd.h>
#include "KPrivateMemory.h"
#include "KProcess.h"

namespace skyline::kernel::type {
    KPrivateMemory::KPrivateMemory(const DeviceState &state, KHandle handle, span<u8> guest, memory::Permission permission, memory::MemoryState memState)
        : permission(permission),
          memoryState(memState),
          handle(handle),
          KMemory(state, KType::KPrivateMemory, guest) {
        if (!state.process->memory.AddressSpaceContains(guest))
            throw exception("KPrivateMemory allocation isn't inside guest address space: 0x{:X} - 0x{:X}", guest.data(), guest.data() + guest.size());
        if (!util::IsPageAligned(guest.data()) || !util::IsPageAligned(guest.size()))
            throw exception("KPrivateMemory mapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", guest.data(), guest.data() + guest.size(), guest.size());

        if (mprotect(guest.data(), guest.size(), PROT_READ | PROT_WRITE | PROT_EXEC) < 0) // We only need to reprotect as the allocation has already been reserved by the MemoryManager
            throw exception("An occurred while mapping private memory: {} with 0x{:X} @ 0x{:X}", strerror(errno), guest.data(), guest.size());

        state.process->memory.InsertChunk(ChunkDescriptor{
            .ptr = guest.data(),
            .size = guest.size(),
            .permission = permission,
            .state = memState,
            .memory = this,
        });
    }

    void KPrivateMemory::Resize(size_t nSize) {
        if (mprotect(guest.data(), nSize, PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
            throw exception("An occurred while resizing private memory: {}", strerror(errno));

        if (nSize < guest.size()) {
            state.process->memory.InsertChunk(ChunkDescriptor{
                .ptr = guest.data() + nSize,
                .size = guest.size() - nSize,
                .state = memory::states::Unmapped,
            });
        } else if (guest.size() < nSize) {
            state.process->memory.InsertChunk(ChunkDescriptor{
                .ptr = guest.data() + guest.size(),
                .size = nSize - guest.size(),
                .permission = permission,
                .state = memoryState,
                .memory = this,
            });
        }
        guest = span<u8>{guest.data(), nSize};
    }

    void KPrivateMemory::Remap(span<u8> map) {
        if (!state.process->memory.AddressSpaceContains(map))
            throw exception("KPrivateMemory remapping isn't inside guest address space: 0x{:X} - 0x{:X}", map.data(), map.end().base());
        if (!util::IsPageAligned(map.data()) || !util::IsPageAligned(map.size()))
            throw exception("KPrivateMemory remapping isn't page-aligned: 0x{:X} - 0x{:X} (0x{:X})", map.data(), map.end().base(), map.size());

        if (mprotect(guest.data(), guest.size(), PROT_NONE) < 0)
            throw exception("An occurred while remapping private memory: {}", strerror(errno));

        if (mprotect(map.data(), map.size(), PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
            throw exception("An occurred while remapping private memory: {}", strerror(errno));
    }

    void KPrivateMemory::UpdatePermission(span<u8> map, memory::Permission pPermission) {
        auto ptr{std::clamp(map.data(), guest.data(), guest.end().base())};
        auto size{std::min(map.size(), static_cast<size_t>((guest.end().base()) - ptr))};

        if (ptr && !util::IsPageAligned(ptr))
            throw exception("KPrivateMemory permission updated with a non-page-aligned address: 0x{:X}", ptr);

        // If a static code region has been mapped as writable it needs to be changed to mutable
        if (memoryState == memory::states::CodeStatic && pPermission.w)
            memoryState = memory::states::CodeMutable;

        state.process->memory.InsertChunk(ChunkDescriptor{
            .ptr = ptr,
            .size = size,
            .permission = pPermission,
            .state = memoryState,
            .memory = this,
        });
    }

    KPrivateMemory::~KPrivateMemory() {
        mprotect(guest.data(), guest.size(), PROT_NONE);
        state.process->memory.InsertChunk(ChunkDescriptor{
            .ptr = guest.data(),
            .size = guest.size(),
            .state = memory::states::Unmapped,
        });
    }
}
