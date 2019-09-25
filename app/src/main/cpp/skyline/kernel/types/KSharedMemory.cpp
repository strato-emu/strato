#include "KSharedMemory.h"
#include "../../nce.h"
#include "../../os.h"
#include <fcntl.h>
#include <unistd.h>

constexpr const char* ASHMEM_NAME_DEF = "dev/ashmem";
constexpr int ASHMEM_SET_SIZE = 0x40087703;

namespace skyline::kernel::type {
    u64 MapFunc(u64 address, size_t size, u64 perms, u64 fd) {
        return reinterpret_cast<u64>(mmap(reinterpret_cast<void *>(address), size, static_cast<int>(perms), MAP_SHARED | ((address) ? MAP_FIXED : 0), static_cast<int>(fd), 0)); // NOLINT(hicpp-signed-bitwise)
    }

    KSharedMemory::KSharedMemory(handle_t handle, pid_t pid, const DeviceState &state, size_t size, const memory::Permission localPermission, const memory::Permission remotePermission, memory::Type type) : size(size), localPermission(localPermission), remotePermission(remotePermission), type(type), KObject(handle, pid, state, KType::KSharedMemory) {
        fd = open(ASHMEM_NAME_DEF, O_RDWR | O_CLOEXEC); // NOLINT(hicpp-signed-bitwise)
        if (fd < 0)
            throw exception(fmt::format("An error occurred while opening {}: {}", ASHMEM_NAME_DEF, fd));
        if (ioctl(fd, ASHMEM_SET_SIZE, size) < 0) // NOLINT(hicpp-signed-bitwise)
            throw exception(fmt::format("An error occurred while setting shared memory size: {}", size));
    }

    void KSharedMemory::Map(u64 address) {
        this->address = address;
        for (auto process : state.os->processVec) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = this->address;
            fregs.regs[1] = size;
            if (process == ownerPid)
                fregs.regs[2] = static_cast<u64 >(localPermission.Get());
            else
                fregs.regs[2] = static_cast<u64>(remotePermission.Get());
            fregs.regs[3] = static_cast<u64>(fd);
            state.nce->ExecuteFunction(reinterpret_cast<void *>(MapFunc), fregs, process);
            if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                throw exception("An error occurred while mapping shared region in child process");
            if (!this->address) this->address = fregs.regs[0];
        }
        this->address = MapFunc(this->address, size, static_cast<u64>(ownerPid ? remotePermission.Get() : localPermission.Get()), static_cast<u64>(fd));
        if (this->address == reinterpret_cast<u64>(MAP_FAILED)) // NOLINT(hicpp-signed-bitwise)
            throw exception(fmt::format("An occurred while mapping shared region: {}", strerror(errno)));
    }

    u64 UnmapFunc(u64 address, size_t size) {
        return static_cast<u64>(munmap(reinterpret_cast<void *>(address), size));
    }

    KSharedMemory::~KSharedMemory() {
        for (auto process : state.os->processVec) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = address;
            fregs.regs[1] = size;
            state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapFunc), fregs, process);
        }
        UnmapFunc(address, size);
        close(fd);
    }

    u64 RemapFunc(u64 address, size_t oldSize, size_t size) {
        return reinterpret_cast<u64>(mremap(reinterpret_cast<void *>(address), oldSize, size, 0));
    }

    void KSharedMemory::Resize(size_t newSize) {
        for (auto process : state.os->processVec) {
            user_pt_regs fregs = {0};
            fregs.regs[0] = address;
            fregs.regs[1] = size;
            fregs.regs[2] = newSize;
            state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapFunc), fregs, process);
            if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
                throw exception("An error occurred while remapping shared region in child process");
        }
        if (RemapFunc(address, size, newSize) == reinterpret_cast<u64>(MAP_FAILED))
            throw exception(fmt::format("An occurred while remapping shared region: {}", strerror(errno)));
        size = newSize;
    }

    u64 UpdatePermissionFunc(u64 address, size_t size, u64 perms) {
        return static_cast<u64>(mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms)));
    }

    void KSharedMemory::UpdatePermission(bool local, memory::Permission newPerms) {
        for (auto process : state.os->processVec) {
            if ((local && process == ownerPid) || (!local && process != ownerPid)) {
                user_pt_regs fregs = {0};
                fregs.regs[0] = address;
                fregs.regs[1] = size;
                fregs.regs[2] = static_cast<u64>(newPerms.Get());
                state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionFunc), fregs, process);
                if (static_cast<int>(fregs.regs[0]) == -1)
                    throw exception("An error occurred while updating shared region's permissions in child process");
            }
        }
        if ((local && ownerPid == 0) || (!local && ownerPid != 0))
            if (mprotect(reinterpret_cast<void *>(address), size, newPerms.Get()) == -1)
                throw exception(fmt::format("An occurred while updating shared region's permissions: {}", strerror(errno)));
        if (local)
            localPermission = newPerms;
        else
            remotePermission = newPerms;
    }

    void KSharedMemory::InitiateProcess(pid_t pid) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<u64>(remotePermission.Get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionFunc), fregs, pid);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while setting shared region's permissions in child process");
    }

    memory::MemoryInfo KSharedMemory::GetInfo(pid_t pid) {
        memory::MemoryInfo info{};
        info.baseAddress = address;
        info.size = size;
        info.type = static_cast<u64>(type);
        info.memoryAttribute.isIpcLocked = (info.ipcRefCount > 0);
        info.memoryAttribute.isDeviceShared = (info.deviceRefCount > 0);
        info.perms = (pid == ownerPid) ? localPermission : remotePermission;
        info.ipcRefCount = ipcRefCount;
        info.deviceRefCount = deviceRefCount;
        return info;
    }
};
