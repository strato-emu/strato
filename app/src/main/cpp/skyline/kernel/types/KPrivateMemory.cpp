#include "KPrivateMemory.h"
#include <nce.h>

namespace skyline::kernel::type {
    u64 MapPrivateFunc(u64 dstAddress, u64 srcAddress, size_t size, u64 perms) {
        dstAddress = reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(dstAddress), size, static_cast<int>(perms), MAP_PRIVATE | MAP_ANONYMOUS | ((dstAddress) ? MAP_FIXED : 0), -1, 0)); // NOLINT(hicpp-signed-bitwise)
        if (srcAddress) {
            memcpy(reinterpret_cast<void *>(dstAddress), reinterpret_cast<const void *>(srcAddress), size);
            mprotect(reinterpret_cast<void *>(srcAddress), size, PROT_NONE);
        }
        return dstAddress;
    }

    KPrivateMemory::KPrivateMemory(const DeviceState &state, pid_t pid, u64 dstAddress, u64 srcAddress, size_t size, memory::Permission permission, const memory::Type type) : state(state), owner(pid), address(dstAddress), size(size), permission(permission), type(type), KObject(state, KType::KPrivateMemory) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = dstAddress;
        fregs.regs[1] = srcAddress;
        fregs.regs[2] = size;
        fregs.regs[3] = static_cast<u64>(permission.Get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(MapPrivateFunc), fregs, pid);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while mapping private region in child process");
        if (!this->address)
            this->address = fregs.regs[0];
    }

    u64 RemapPrivateFunc(u64 address, size_t oldSize, size_t size) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), oldSize, size, 0));
    }

    void KPrivateMemory::Resize(size_t newSize) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = newSize;
        state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapPrivateFunc), fregs, owner);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while remapping private region in child process");
        size = newSize;
    }

    u64 UpdatePermissionPrivateFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KPrivateMemory::UpdatePermission(memory::Permission newPerms) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<u64>(newPerms.Get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionPrivateFunc), fregs, owner);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while updating private region's permissions in child process");
        permission = newPerms;
    }

    memory::MemoryInfo KPrivateMemory::GetInfo() {
        memory::MemoryInfo info{};
        info.baseAddress = address;
        info.size = size;
        info.type = static_cast<u64>(type);
        info.memoryAttribute.isIpcLocked = (info.ipcRefCount > 0);
        info.memoryAttribute.isDeviceShared = (info.deviceRefCount > 0);
        info.perms = permission;
        info.ipcRefCount = ipcRefCount;
        info.deviceRefCount = deviceRefCount;
        return info;
    }

    u64 UnmapPrivateFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    KPrivateMemory::~KPrivateMemory() {
        try {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapPrivateFunc), fregs, owner);
        } catch (const std::exception&) {}
    }
};
