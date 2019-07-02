#include "cpu.h"
#include "memory.h"

// TODO: Handle Unicorn errors
namespace core {
    Cpu* currentContext;
    void HookInterrupt(uc_engine *uc, uint32_t intno, void *user_data);

    Cpu::Cpu() {
        uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc);

        uc_hook hook{};
        uc_hook_add(uc, &hook, UC_HOOK_INTR, (void*)HookInterrupt, this, 1, 0);

        // Map stack memory
        memory::Map(uc, 0x3000000, 0x1000000, "stack");
        SetRegister(UC_ARM64_REG_SP, 0x3100000);
        memory::Map(uc, 0x2000000, 0x1000, "tls");
        SetRegister(UC_ARM64_REG_TPIDRRO_EL0, 0x2000000);

        currentContext = this;
    }

    void Cpu::Run(uint64_t address) {
        uc_err err = uc_emu_start(uc, address, 1ULL << 63, 0, 0);
        if(err)
            syslog(LOG_ERR, "uc_emu_start failed: %s", uc_strerror(err));
    }

    uint64_t Cpu::GetRegister(uint32_t regid) {
        uint64_t registerValue;
        uc_reg_read(uc, regid, &registerValue);
        return registerValue;
    }

    void Cpu::SetRegister(uint32_t regid, uint64_t value) {
        uc_reg_write(uc, regid, &value);
    }

    void HookInterrupt(uc_engine *uc, uint32_t intno, void *user_data) {
        syslog(LOG_WARNING, "Interrupt called at x%x", currentContext->GetRegister(UC_ARM64_REG_PC));
        if (intno == 2) {
            uint32_t instr{};
            uc_mem_read(uc, currentContext->GetRegister(UC_ARM64_REG_PC) - 4, &instr, 4);

            uint32_t svcId = instr >> 5 & 0xFF;

            // TODO: Handle SVCs
            syslog(LOG_DEBUG, "SVC 0x%x called!", svcId);
        } else {
            syslog(LOG_ERR, "Unhandled interrupt #%i", intno);
            uc_close(uc);
        }
    }

    Cpu* CpuContext() { return currentContext; }
}