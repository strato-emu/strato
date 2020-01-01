#include "KPrivateMemory.h"
#include "KProcess.h"
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

    KPrivateMemory::KPrivateMemory(const DeviceState &state, u64 dstAddress, u64 srcAddress, size_t size, memory::Permission permission, const memory::Type type, const pid_t pid) : state(state), address(dstAddress), size(size), permission(permission), type(type), KObject(state, KType::KPrivateMemory) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = dstAddress;
        fregs.regs[1] = srcAddress;
        fregs.regs[2] = size;
        fregs.regs[3] = static_cast<u64>(permission.Get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(MapPrivateFunc), fregs, pid ? pid : state.process->pid);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while mapping private region in child process");
        if (!this->address)
            this->address = fregs.regs[0];
    }

    u64 RemapPrivateFunc(u64 address, size_t oldSize, size_t size, u64 flags) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), oldSize, size, static_cast<int>(flags)));
    }

    u64 KPrivateMemory::Resize(size_t newSize, bool canMove) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = newSize;
        fregs.regs[3] = canMove ? MREMAP_MAYMOVE : 0;
        state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapPrivateFunc), fregs, state.process->pid);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while remapping private region in child process");
        address = fregs.regs[0];
        size = newSize;
        return address;
    }

    u64 UpdatePermissionPrivateFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KPrivateMemory::UpdatePermission(memory::Permission permission) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<u64>(permission.Get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionPrivateFunc), fregs, state.process->pid);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while updating private region's permissions in child process");
        this->permission = permission;
    }

    memory::MemoryInfo KPrivateMemory::GetInfo(u64 address) {
        memory::MemoryInfo info{};
        info.baseAddress = address;
        info.size = size;
        info.type = static_cast<u32>(type);
        for (const auto &region : regionInfoVec)
            if ((address >= region.address) && (address < (region.address + region.size)))
                info.memoryAttribute.isUncached = region.isUncached;
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
            user_pt_regs fregs = {0};
            fregs.regs[0] = address;
            fregs.regs[1] = size;
            state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapPrivateFunc), fregs, state.process->pid);
        } catch (const std::exception &) {
        }
    }
};
