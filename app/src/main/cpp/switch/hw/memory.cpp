#include <sys/mman.h>
#include <cerrno>
#include "memory.h"

namespace lightSwitch::hw {
    Memory::Memory() {
        // Map TLS memory
        Memory::Map(constant::tls_addr, constant::tls_size, tls);
    }

    void Memory::Map(uint64_t address, size_t size, Region region) {
        void *ptr = mmap((void *) address, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON | MAP_FIXED, 0, 0);
        if (ptr == MAP_FAILED)
            throw exception("An occurred while mapping region: " + std::string(strerror(errno)));

        region_map.insert(std::pair<Region, RegionData>(region, {address, size}));
    }

    void Memory::Remap(Region region, size_t size) {
        RegionData region_data = region_map.at(region);

        void *ptr = mremap(reinterpret_cast<void *>(region_data.address), region_data.size, size, 0);
        if (ptr == MAP_FAILED)
            throw exception("An occurred while remapping region: " + std::string(strerror(errno)));

        region_map[region].size = size;
    }

    void Memory::Unmap(Region region) {
        RegionData region_data = region_map.at(region);

        int err = munmap(reinterpret_cast<void *>(region_data.address), region_data.size);
        if (err == -1)
            throw exception("An occurred while unmapping region: " + std::string(strerror(errno)));
    }

    void Memory::Write(void *data, uint64_t offset, size_t size) {
        std::memcpy(reinterpret_cast<void *>(offset), data, size);
    }

    template<typename T>
    void Memory::Write(T value, uint64_t offset) {
        Write(reinterpret_cast<void *>(&value), offset, sizeof(T));
    }

    void Memory::Read(void *destination, uint64_t offset, size_t size) {
        std::memcpy(destination, reinterpret_cast<void *>(offset), size);
    }

    template<typename T>
    T Memory::Read(uint64_t offset) {
        T value;
        Read(reinterpret_cast<void *>(&value), offset, sizeof(T));
        return value;
    }
}