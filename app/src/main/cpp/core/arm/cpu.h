#pragma once
#include <syslog.h>
#include <unicorn/unicorn.h>

namespace core::cpu {
    bool Initialize();
    void Run(uint64_t address);

    uint64_t GetRegister(uint32_t regid);
    void SetRegister(uint32_t regid, uint64_t value);

//    bool MapUnicorn(uint64_t address, size_t size);
}