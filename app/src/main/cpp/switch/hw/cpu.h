#pragma once

#include <syslog.h>
#include <unicorn/unicorn.h>

namespace lightSwitch::hw {
    class Cpu {
    private:
        uc_engine *uc;
        uc_err err;

    public:
        Cpu();

        ~Cpu();

        void SetHook(void *HookInterrupt);

        void Execute(uint64_t address);

        void StopExecution();

        uint64_t GetRegister(uint32_t regid);

        void SetRegister(uint32_t regid, uint64_t value);

        uc_engine *GetEngine();
    };
}
