#pragma once
#include <string>
#include <unicorn/unicorn.h>

#define MEM_BASE 0x80000000

namespace mem {
    struct MemoryRegion {
        std::string label;
        uint64_t address;
        size_t size;
        void* ptr;
    };

    bool Map(uc_engine* uc, uint64_t address, size_t size, std::string label="");

    void Write(void* data, uint64_t offset, size_t size);
    void WriteU8(uint8_t value, uint64_t offset);
    void WriteU16(uint16_t value, uint64_t offset);
    void WriteU32(uint32_t value, uint64_t offset);
    void WriteU64(uint64_t value, uint64_t offset);

    void Read(void* destination, uint64_t offset, size_t size);
    uint8_t ReadU8(uint64_t offset);
    uint16_t ReadU16(uint64_t offset);
    uint32_t ReadU32(uint64_t offset);
    uint64_t ReadU64(uint64_t offset);
}