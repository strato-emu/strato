#include "KPrivateMemory.h"
#include "../../nce.h"
#include "../../os.h"
#include <android/sharedmem.h>
#include <fcntl.h>
#include <unistd.h>

namespace lightSwitch::kernel::type {
    u64 MapPrivateFunc(u64 dst_address, u64 src_address, size_t size, u64 perms) {
        dst_address = reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(dst_address), size, static_cast<int>(perms), MAP_PRIVATE | MAP_ANONYMOUS | ((dst_address) ? MAP_FIXED : 0), -1, 0)); // NOLINT(hicpp-signed-bitwise)
        if (src_address) {
            memcpy(reinterpret_cast<void *>(dst_address), reinterpret_cast<const void *>(src_address), size);
            mprotect(reinterpret_cast<void *>(src_address), size, PROT_NONE);
        }
        return dst_address;
    }

    KPrivateMemory::KPrivateMemory(const device_state &state, u64 dst_address, u64 src_address, size_t size, Memory::Permission permission, const Memory::Type type, pid_t owner_pid) : state(state), address(dst_address), size(size), permission(permission), type(type), owner_pid(owner_pid) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = dst_address;
        fregs.regs[1] = src_address;
        fregs.regs[2] = size;
        fregs.regs[3] = static_cast<u64>(permission.get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(MapPrivateFunc), fregs, owner_pid);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while mapping private region in child process");
        if (!this->address) this->address = fregs.regs[0];
    }

    u64 UnmapPrivateFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    u64 RemapPrivateFunc(u64 address, size_t old_size, size_t size) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), old_size, size, 0));
    }

    void KPrivateMemory::Resize(size_t new_size) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = new_size;
        state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapPrivateFunc), fregs, owner_pid);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while remapping private region in child process");
        size = new_size;
    }

    u64 UpdatePermissionPrivateFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KPrivateMemory::UpdatePermission(Memory::Permission new_perms) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<u64>(new_perms.get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionPrivateFunc), fregs, owner_pid);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while updating private region's permissions in child process");
        permission = new_perms;
    }

    Memory::MemoryInfo KPrivateMemory::GetInfo() {
        Memory::MemoryInfo info{};
        info.base_address = address;
        info.size = size;
        info.type = static_cast<u64>(type);
        info.memory_attribute.IsIpcLocked = (info.ipc_ref_count > 0);
        info.memory_attribute.IsDeviceShared = (info.device_ref_count > 0);
        info.perms = permission;
        info.ipc_ref_count = ipc_ref_count;
        info.device_ref_count = device_ref_count;
        return info;
    }
};
