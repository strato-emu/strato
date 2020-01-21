#include "KSharedMemory.h"
#include "KProcess.h"
#include <os.h>
#include <android/sharedmem.h>
#include <unistd.h>
#include <asm/unistd.h>

namespace skyline::kernel::type {
    KSharedMemory::KSharedMemory(const DeviceState &state, u64 address, size_t size, const memory::Permission permission, memory::MemoryState memState) : initialState(memState), KMemory(state, KType::KSharedMemory) {
        fd = ASharedMemory_create("", size);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);
        address = reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED | ((address) ? MAP_FIXED : 0), fd, 0));
        if (address == reinterpret_cast<u64>(MAP_FAILED))
            throw exception("An occurred while mapping shared memory: {}", strerror(errno));
        kernel = {.address = address, .size = size, .permission = permission};
    }

    u64 KSharedMemory::Map(const u64 address, const u64 size, memory::Permission permission) {
        Registers fregs{};
        fregs.x0 = address;
        fregs.x1 = size;
        fregs.x2 = static_cast<u64>(permission.Get());
        fregs.x3 = static_cast<u64>(MAP_SHARED | ((address) ? MAP_FIXED : 0));
        fregs.x4 = static_cast<u64>(fd);
        fregs.x8 = __NR_mmap;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while mapping shared memory in guest");
        guest = {.address = fregs.x0, .size = size, .permission = permission};
        ChunkDescriptor chunk{
            .address = fregs.x0,
            .size = size,
            .state = initialState,
        };
        BlockDescriptor block{
            .address = fregs.x0,
            .size = size,
            .permission = permission,
        };
        chunk.blockList.push_front(block);
        state.os->memory.InsertChunk(chunk);
        return fregs.x0;
    }

    void KSharedMemory::Resize(size_t size) {
        if (guest.valid()) {
            Registers fregs{};
            fregs.x0 = guest.address;
            fregs.x1 = guest.size;
            fregs.x2 = size;
            fregs.x8 = __NR_mremap;
            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while remapping shared memory in guest");
            guest.size = size;
            auto chunk = state.os->memory.GetChunk(guest.address);
            MemoryManager::ResizeChunk(chunk, size);
        }
        if (kernel.valid()) {
            if (mremap(reinterpret_cast<void *>(kernel.address), kernel.size, size, 0) == MAP_FAILED)
                throw exception("An error occurred while remapping shared region: {}", strerror(errno));
            kernel.size = size;
        }
    }

    void KSharedMemory::UpdatePermission(u64 address, u64 size, memory::Permission permission, bool host) {
        if (guest.valid() && !host) {
            Registers fregs{};
            fregs.x0 = address;
            fregs.x1 = size;
            fregs.x2 = static_cast<u64>(permission.Get());
            fregs.x8 = __NR_mprotect;
            state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            if (fregs.x0 < 0)
                throw exception("An error occurred while updating shared memory's permissions in guest");
            auto chunk = state.os->memory.GetChunk(address);
            BlockDescriptor block{
                .address = address,
                .size = size,
                .permission = permission,
            };
            MemoryManager::InsertBlock(chunk, block);
        }
        if (kernel.valid() && host) {
            if (mprotect(reinterpret_cast<void *>(kernel.address), kernel.size, permission.Get()) == reinterpret_cast<u64>(MAP_FAILED))
                throw exception("An error occurred while remapping shared memory: {}", strerror(errno));
            kernel.permission = permission;
        }
    }

    KSharedMemory::~KSharedMemory() {
        try {
            if (guest.valid() && state.process) {
                Registers fregs{};
                fregs.x0 = guest.address;
                fregs.x1 = guest.size;
                fregs.x8 = __NR_munmap;
                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            }
            if (kernel.valid())
                munmap(reinterpret_cast<void *>(kernel.address), kernel.size);
        } catch (const std::exception &) {
        }
        state.os->memory.DeleteChunk(guest.address);
        close(fd);
    }
};
