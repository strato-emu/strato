#include "KSharedMemory.h"
#include <nce.h>
#include <android/sharedmem.h>
#include <unistd.h>

namespace skyline::kernel::type {
    u64 MapSharedFunc(u64 address, size_t size, u64 perms, u64 fd) {
        return reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, static_cast<int>(perms), MAP_SHARED | ((address) ? MAP_FIXED : 0), static_cast<int>(fd), 0)); // NOLINT(hicpp-signed-bitwise)
    }

    KSharedMemory::KSharedMemory(const DeviceState &state, u64 address, size_t size, const memory::Permission permission, memory::Type type) : type(type), KObject(state, KType::KSharedMemory) {
        fd = ASharedMemory_create("", size);
        if (fd < 0)
            throw exception("An error occurred while creating shared memory: {}", fd);
        address = MapSharedFunc(address, size, static_cast<u64>(permission.Get()), static_cast<u64>(fd));
        if (address == reinterpret_cast<u64>(MAP_FAILED)) // NOLINT(hicpp-signed-bitwise)
            throw exception("An occurred while mapping shared region: {}", strerror(errno));
        procInfMap[0] = {address, size, permission};
    }

    u64 KSharedMemory::Map(u64 address, u64 size, memory::Permission permission, pid_t pid) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<u64>(permission.Get());
        fregs.regs[3] = static_cast<u64>(fd);
        state.nce->ExecuteFunction(reinterpret_cast<void *>(MapSharedFunc), fregs, pid);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while mapping shared region in child process");
        procInfMap[pid] = {fregs.regs[0], size, permission};
        return fregs.regs[0];
    }

    u64 UnmapSharedFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    KSharedMemory::~KSharedMemory() {
        for (auto[process, procInf] : procInfMap) {
            try {
                if(process) {
                    user_pt_regs fregs = {0};
                    fregs.regs[0] = procInf.address;
                    fregs.regs[1] = procInf.size;
                    state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapSharedFunc), fregs, process);
                } else
                    UnmapSharedFunc(procInf.address, procInf.size);
            } catch (const std::exception &) {}
        }
        close(fd);
    }

    u64 RemapSharedFunc(u64 address, size_t oldSize, size_t size) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), oldSize, size, 0));
    }

    void KSharedMemory::Resize(size_t newSize) {
        for (auto&[process, procInf] : procInfMap) {
            if(process) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = procInf.address;
                fregs.regs[1] = procInf.size;
                fregs.regs[2] = newSize;
                state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapSharedFunc), fregs, process);
                if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                    throw exception("An error occurred while remapping shared region in child process");
            } else {
                if (RemapSharedFunc(procInf.address, procInf.size, newSize) == reinterpret_cast<u64>(MAP_FAILED))
                    throw exception("An occurred while remapping shared region: {}", strerror(errno));
            }
            procInf.size = newSize;
        }
    }

    u64 UpdatePermissionSharedFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KSharedMemory::UpdatePermission(pid_t pid, memory::Permission permission) {
        for (auto&[process, procInf] : procInfMap) {
            if(process) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = procInf.address;
                fregs.regs[1] = procInf.size;
                fregs.regs[2] = static_cast<u64>(procInf.permission.Get());
                state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionSharedFunc), fregs, process);
                if (static_cast<int>(fregs.regs[0]) == -1)
                    throw exception("An error occurred while updating shared region's permissions in child process");
            } else {
                if (UpdatePermissionSharedFunc(procInf.address, procInf.size, static_cast<u64>(permission.Get())) == reinterpret_cast<u64>(MAP_FAILED))
                    throw exception("An occurred while remapping shared region: {}", strerror(errno));
            }
            procInf.permission = permission;
        }
    }

    memory::MemoryInfo KSharedMemory::GetInfo(pid_t pid) {
        memory::MemoryInfo info{};
        const auto &procInf = procInfMap.at(pid);
        info.baseAddress = procInf.address;
        info.size = procInf.size;
        info.type = static_cast<u64>(type);
        info.memoryAttribute.isIpcLocked = (info.ipcRefCount > 0);
        info.memoryAttribute.isDeviceShared = (info.deviceRefCount > 0);
        info.perms = procInf.permission;
        info.ipcRefCount = ipcRefCount;
        info.deviceRefCount = deviceRefCount;
        return info;
    }
};
