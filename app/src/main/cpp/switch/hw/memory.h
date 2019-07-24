#pragma once

#include <string>
#include <unicorn/unicorn.h>
#include <map>

namespace lightSwitch::hw {
    class Memory {
    private:
        uc_engine *uc;
    public:
        enum Region {
            stack, tls, text, rodata, data, bss
        };
        struct RegionData {
            uint64_t address;
            size_t size;
        };
        std::map<Region, RegionData> region_map;

        Memory(uc_engine *uc_);

        void Map(uint64_t address, size_t size, Region region);

        void Write(void *data, uint64_t offset, size_t size);

        template<typename T>
        void Write(T value, uint64_t offset);

        void Read(void *destination, uint64_t offset, size_t size);

        template<typename T>
        T Read(uint64_t offset);
    };

}