#pragma once
#include <syslog.h>
#include <unicorn/unicorn.h>

namespace core {
    class Cpu {
    public:
        Cpu();
        ~Cpu() { uc_close(uc); };

        void Run(uint64_t address);

        uint64_t GetRegister(uint32_t regid);
        void SetRegister(uint32_t regid, uint64_t value);

        uc_engine *uc;
    private:
        void HookInterrupt(uc_engine *uc, uint32_t intno, void *user_data);
    };
}