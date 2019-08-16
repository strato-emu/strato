#include <sys/mman.h>
#include <syslog.h>
#include <vector>
#include "memory.h"
#include "../constant.h"

namespace lightSwitch::hw {
    // TODO: Boundary checks
    Memory::Memory() {
        // Map stack memory
        // Memory::Map(constant::stack_addr, constant::stack_size, stack);
        // Map TLS memory
        Memory::Map(constant::tls_addr, constant::tls_size, tls);
    }

    void Memory::Map(uint64_t address, size_t size, Region region) {
        region_map.insert(std::pair<Region, RegionData>(region, {address, size}));
        void *ptr = mmap((void *) address, size, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON | MAP_FIXED, 0, 0);
        if (!ptr)
            throw exception("An occurred while mapping region");
    }

    void Memory::Write(void *data, uint64_t offset, size_t size) {
        std::memcpy((void *) offset, data, size);
    }

    template<typename T>
    void Memory::Write(T value, uint64_t offset) {
        Write(reinterpret_cast<void *>(&value), offset, sizeof(T));
    }

    void Memory::Read(void *destination, uint64_t offset, size_t size) {
        std::memcpy(destination, (void *) (offset), size);
    }

    template<typename T>
    T Memory::Read(uint64_t offset) {
        T value;
        Read(reinterpret_cast<void *>(&value), offset, sizeof(T));
        return value;
    }
}