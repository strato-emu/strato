#include "KPrivateMemory.h"
#include "KProcess.h"
#include <nce.h>
#include <asm/unistd.h>

namespace skyline::kernel::type {
    KPrivateMemory::KPrivateMemory(const DeviceState &state, u64 dstAddress, size_t size, memory::Permission permission, const memory::Type type, std::shared_ptr<KThread> thread) : state(state), address(dstAddress), size(size), permission(permission), type(type), KObject(state, KType::KPrivateMemory) {
        Registers fregs{};
        fregs.x0 = dstAddress;
        fregs.x1 = size;
        fregs.x2 = static_cast<u64>(permission.Get());
        fregs.x3 = static_cast<u64>(MAP_PRIVATE | MAP_ANONYMOUS | ((dstAddress) ? MAP_FIXED : 0));
        fregs.x4 = static_cast<u64>(-1);
        fregs.x8 = __NR_mmap;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs, thread);
        if (fregs.x0 < 0)
            throw exception("An error occurred while mapping private region in child process");
        if (!this->address)
            this->address = fregs.x0;
    }

    u64 RemapPrivateFunc(u64 address, size_t oldSize, size_t size, u64 flags) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), oldSize, size, static_cast<int>(flags)));
    }

    u64 KPrivateMemory::Resize(size_t newSize, bool canMove) {
        Registers fregs{};
        fregs.x0 = address;
        fregs.x1 = size;
        fregs.x2 = newSize;
        fregs.x3 = canMove ? MREMAP_MAYMOVE : 0;
        fregs.x8 = __NR_mremap;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs, state.thread);
        if (fregs.x0 < 0)
            throw exception("An error occurred while remapping private region in child process");
        address = fregs.x0;
        size = newSize;
        return address;
    }

    u64 UpdatePermissionPrivateFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KPrivateMemory::UpdatePermission(memory::Permission permission) {
        Registers fregs{};
        fregs.x0 = address;
        fregs.x1 = size;
        fregs.x2 = static_cast<u64>(permission.Get());
        fregs.x8 = __NR_mprotect;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs, state.thread);
        if (fregs.x0 < 0)
            throw exception("An error occurred while updating private region's permissions in child process");
        this->permission = permission;
    }

    memory::MemoryInfo KPrivateMemory::GetInfo(u64 address) {
        memory::MemoryInfo info{};
        info.baseAddress = address;
        info.size = size;
        info.type = static_cast<u32>(type);
        for (const auto &region : regionInfoVec)
            if ((address >= region.address) && (address < (region.address + region.size))) {
                info.memoryAttribute.isUncached = region.isUncached;
            }
        info.memoryAttribute.isIpcLocked = (info.ipcRefCount > 0);
        info.memoryAttribute.isDeviceShared = (info.deviceRefCount > 0);
        info.r = permission.r;
        info.w = permission.w;
        info.x = permission.x;
        info.ipcRefCount = ipcRefCount;
        info.deviceRefCount = deviceRefCount;
        return info;
    }

    u64 UnmapPrivateFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    KPrivateMemory::~KPrivateMemory() {
        try {
            if (state.process) {
                Registers fregs{};
                fregs.x0 = address;
                fregs.x1 = size;
                fregs.x8 = __NR_munmap;
                state.nce->ExecuteFunction(ThreadCall::Syscall, fregs, state.process->pid);
            }
        } catch (const std::exception &) {
        }
    }
};
