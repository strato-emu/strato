#include "KPrivateMemory.h"
#include "KProcess.h"
#include <os.h>
#include <asm/unistd.h>

namespace skyline::kernel::type {
    KPrivateMemory::KPrivateMemory(const DeviceState &state, u64 address, size_t size, memory::Permission permission, const memory::MemoryState memState) : state(state), address(address), size(size), KMemory(state, KType::KPrivateMemory) {
        Registers fregs{};
        fregs.x0 = address;
        fregs.x1 = size;
        fregs.x2 = static_cast<u64>(permission.Get());
        fregs.x3 = static_cast<u64>(MAP_PRIVATE | MAP_ANONYMOUS | ((address) ? MAP_FIXED : 0));
        fregs.x4 = static_cast<u64>(-1);
        fregs.x8 = __NR_mmap;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while mapping private section in child process");
        if (!this->address)
            this->address = fregs.x0;
        BlockDescriptor block{
            .address = fregs.x0,
            .size = size,
            .permission = permission,
        };
        ChunkDescriptor chunk{
            .address = fregs.x0,
            .size = size,
            .state = memState,
            .blockList = {block},
        };
        state.os->memory.InsertChunk(chunk);
    }

    void KPrivateMemory::Resize(size_t nSize) {
        Registers fregs{};
        fregs.x0 = address;
        fregs.x1 = size;
        fregs.x2 = nSize;
        fregs.x8 = __NR_mremap;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while remapping private section in child process");
        size = nSize;
        auto chunk = state.os->memory.GetChunk(address);
        MemoryManager::ResizeChunk(chunk, size);
    }

    void KPrivateMemory::UpdatePermission(const u64 address, const u64 size, memory::Permission permission) {
        Registers fregs{};
        fregs.x0 = address;
        fregs.x1 = size;
        fregs.x2 = static_cast<u64>(permission.Get());
        fregs.x8 = __NR_mprotect;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        if (fregs.x0 < 0)
            throw exception("An error occurred while updating private section's permissions in child process");
        auto chunk = state.os->memory.GetChunk(address);
        BlockDescriptor block{
                .address = address,
                .size = size,
                .permission = permission,
        };
        MemoryManager::InsertBlock(chunk, block);
    }

    KPrivateMemory::~KPrivateMemory() {
        try {
            if (state.process) {
                Registers fregs{};
                fregs.x0 = address;
                fregs.x1 = size;
                fregs.x8 = __NR_munmap;
                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
            }
        } catch (const std::exception &) {
        }
        state.os->memory.DeleteChunk(address);
    }
};
