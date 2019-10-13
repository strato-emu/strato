#include "KSharedMemory.h"
#include <nce.h>
#include <fcntl.h>
#include <unistd.h>

constexpr const char* ASHMEM_NAME_DEF = "dev/ashmem";
constexpr int ASHMEM_SET_SIZE = 0x40087703;

namespace skyline::kernel::type {
    u64 MapSharedFunc(u64 address, size_t size, u64 perms, u64 fd) {
        return reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, static_cast<int>(perms), MAP_SHARED | ((address) ? MAP_FIXED : 0), static_cast<int>(fd), 0)); // NOLINT(hicpp-signed-bitwise)
    }

    KSharedMemory::KSharedMemory(const DeviceState &state, pid_t pid, u64 kaddress, size_t ksize, const memory::Permission localPermission, const memory::Permission remotePermission, memory::Type type) : kaddress(kaddress), ksize(ksize), localPermission(localPermission), remotePermission(remotePermission), type(type), owner(pid), KObject(state, KType::KSharedMemory) {
        fd = open(ASHMEM_NAME_DEF, O_RDWR | O_CLOEXEC); // NOLINT(hicpp-signed-bitwise)
        if (fd < 0)
            throw exception(fmt::format("An error occurred while opening {}: {}", ASHMEM_NAME_DEF, fd));
        if (ioctl(fd, ASHMEM_SET_SIZE, ksize) < 0) // NOLINT(hicpp-signed-bitwise)
            throw exception(fmt::format("An error occurred while setting shared memory size: {}", ksize));
        kaddress = MapSharedFunc(kaddress, ksize, static_cast<u64>(pid ? remotePermission.Get() : localPermission.Get()), static_cast<u64>(fd));
        if (kaddress == reinterpret_cast<u64>(MAP_FAILED)) // NOLINT(hicpp-signed-bitwise)
            throw exception(fmt::format("An occurred while mapping shared region: {}", strerror(errno)));
    }

    u64 KSharedMemory::Map(u64 address, u64 size, pid_t process) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        if (process == owner)
            fregs.regs[2] = static_cast<u64 >(localPermission.Get());
        else
            fregs.regs[2] = static_cast<u64>(remotePermission.Get());
        fregs.regs[3] = static_cast<u64>(fd);
        state.nce->ExecuteFunction(reinterpret_cast<void *>(MapSharedFunc), fregs, process);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while mapping shared region in child process");
        procInfMap[process] = {fregs.regs[0], size};
        return fregs.regs[0];
    }

    u64 UnmapSharedFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    KSharedMemory::~KSharedMemory() {
        for (auto [process, procInf] : procInfMap) {
            try {
                user_pt_regs fregs = {0};
                fregs.regs[0] = procInf.address;
                fregs.regs[1] = procInf.size;
                state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapSharedFunc), fregs, process);
            } catch (const std::exception&) {}
        }
        UnmapSharedFunc(kaddress, ksize);
        close(fd);
    }

    u64 RemapSharedFunc(u64 address, size_t oldSize, size_t size) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), oldSize, size, 0));
    }

    void KSharedMemory::Resize(size_t newSize) {
        for (auto& [process, procInf] : procInfMap) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = procInf.address;
            fregs.regs[1] = procInf.size;
            fregs.regs[2] = newSize;
            state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapSharedFunc), fregs, process);
            if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                throw exception("An error occurred while remapping shared region in child process");
            procInf.size = newSize;
        }
        if (RemapSharedFunc(kaddress, ksize, newSize) == reinterpret_cast<u64>(MAP_FAILED))
            throw exception(fmt::format("An occurred while remapping shared region: {}", strerror(errno)));
        ksize = newSize;
    }

    u64 UpdatePermissionSharedFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KSharedMemory::UpdatePermission(bool local, memory::Permission newPerms) {
        for (auto& [process, procInf] : procInfMap) {
            if ((local && process == owner) || (!local && process != owner)) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = procInf.address;
                fregs.regs[1] = procInf.size;
                fregs.regs[2] = static_cast<u64>(newPerms.Get());
                state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionSharedFunc), fregs, process);
                if (static_cast<int>(fregs.regs[0]) == -1)
                    throw exception("An error occurred while updating shared region's permissions in child process");
            }
        }
        if ((local && owner == 0) || (!local && owner != 0))
            if (mprotect(reinterpret_cast<void *>(kaddress), ksize, newPerms.Get()) == -1)
                throw exception(fmt::format("An occurred while updating shared region's permissions: {}", strerror(errno)));
        if (local)
            localPermission = newPerms;
        else
            remotePermission = newPerms;
    }

    memory::MemoryInfo KSharedMemory::GetInfo(pid_t process) {
        memory::MemoryInfo info{};
        info.baseAddress = kaddress;
        info.size = ksize;
        info.type = static_cast<u64>(type);
        info.memoryAttribute.isIpcLocked = (info.ipcRefCount > 0);
        info.memoryAttribute.isDeviceShared = (info.deviceRefCount > 0);
        info.perms = (process == owner) ? localPermission : remotePermission;
        info.ipcRefCount = ipcRefCount;
        info.deviceRefCount = deviceRefCount;
        return info;
    }
};
