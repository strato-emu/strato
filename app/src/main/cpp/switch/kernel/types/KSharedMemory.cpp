#include "KSharedMemory.h"
#include "../../nce.h"
#include "../../os.h"
#include <android/sharedmem.h>
#include <fcntl.h>
#include <unistd.h>

namespace lightSwitch::kernel::type {
    u64 MapFunc(u64 address, size_t size, u64 perms, u64 fd) {
        return reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, static_cast<int>(perms), MAP_SHARED | ((address) ? MAP_FIXED : 0), static_cast<int>(fd), 0)); // NOLINT(hicpp-signed-bitwise)
    }

    KSharedMemory::KSharedMemory(const device_state &state, size_t size, const Memory::Permission local_permission, const Memory::Permission remote_permission, Memory::Type type, handle_t handle, pid_t owner_pid) : state(state), size(size), local_permission(local_permission), remote_permission(remote_permission), type(type), owner_pid(owner_pid), KObject(handle, KObjectType::KSharedMemory) {
        fd = ASharedMemory_create("", size);
    }

    void KSharedMemory::Map(u64 address) {
        this->address = address;
        for (auto process : state.os->process_vec) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = this->address;
            fregs.regs[1] = size;
            if (process == owner_pid)
                fregs.regs[2] = static_cast<u64 >(local_permission.get());
            else
                fregs.regs[2] = static_cast<u64>(remote_permission.get());
            fregs.regs[3] = static_cast<u64>(fd);
            state.nce->ExecuteFunction(reinterpret_cast<void *>(MapFunc), fregs, process);
            if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                throw exception("An error occurred while mapping shared region in child process");
            if (!this->address) this->address = fregs.regs[0];
        }
        this->address = MapFunc(this->address, size, static_cast<u64>(owner_pid ? remote_permission.get() : local_permission.get()), static_cast<u64>(fd));
        if (this->address == reinterpret_cast<u64>(MAP_FAILED)) // NOLINT(hicpp-signed-bitwise)
            throw exception(fmt::format("An occurred while mapping shared region: {}", strerror(errno)));
    }

    u64 UnmapFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    KSharedMemory::~KSharedMemory() {
        for (auto process : state.os->process_vec) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = address;
            fregs.regs[1] = size;
            state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapFunc), fregs, process);
        }
        UnmapFunc(address, size);
        close(fd);
    }

    u64 RemapFunc(u64 address, size_t old_size, size_t size) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), old_size, size, 0));
    }

    void KSharedMemory::Resize(size_t new_size) {
        for (auto process : state.os->process_vec) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = address;
            fregs.regs[1] = size;
            fregs.regs[2] = new_size;
            state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapFunc), fregs, process);
            if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                throw exception("An error occurred while remapping shared region in child process");
        }
        if (RemapFunc(address, size, new_size) == reinterpret_cast<u64>(MAP_FAILED))
            throw exception(fmt::format("An occurred while remapping shared region: {}", strerror(errno)));
        size = new_size;
    }

    u64 UpdatePermissionFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KSharedMemory::UpdatePermission(bool local, Memory::Permission new_perms) {
        for (auto process : state.os->process_vec) {
            if ((local && process == owner_pid) || (!local && process != owner_pid)) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = address;
                fregs.regs[1] = size;
                fregs.regs[2] = static_cast<u64>(new_perms.get());
                state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionFunc), fregs, process);
                if (static_cast<int>(fregs.regs[0]) == -1)
                    throw exception("An error occurred while updating shared region's permissions in child process");
            }
        }
        if ((local && owner_pid == 0) || (!local && owner_pid != 0))
            if (mprotect(reinterpret_cast<void *>(address), size, new_perms.get()) == -1)
                throw exception(fmt::format("An occurred while updating shared region's permissions: {}", strerror(errno)));
        if (local)
            local_permission = new_perms;
        else
            remote_permission = new_perms;
    }

    void KSharedMemory::InitiateProcess(pid_t pid) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<u64>(remote_permission.get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionFunc), fregs, pid);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while setting shared region's permissions in child process");
    }

    Memory::MemoryInfo KSharedMemory::GetInfo(pid_t pid) {
        Memory::MemoryInfo info{};
        info.base_address = address;
        info.size = size;
        info.type = static_cast<u64>(type);
        info.memory_attribute.IsIpcLocked = (info.ipc_ref_count > 0);
        info.memory_attribute.IsDeviceShared = (info.device_ref_count > 0);
        info.perms = (pid == owner_pid) ? local_permission : remote_permission;
        info.ipc_ref_count = ipc_ref_count;
        info.device_ref_count = device_ref_count;
        return info;
    }
};
