#include "cpu.h"

// TODO: Handle Unicorn errors
namespace core {
    Cpu::Cpu() {
        uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc);

        uc_hook hook{};
        uc_hook_add(uc, &hook, UC_HOOK_INTR, (void*)HookInterrupt, this, 0, -1);
    }

    void Cpu::Run(uint64_t address) {
        uc_emu_start(uc, address, 1ULL << 63, 0, 0);
    }

    uint64_t Cpu::GetRegister(uint32_t regid) {
        uint64_t registerValue;
        uc_reg_read(uc, regid, &registerValue);
        return registerValue;
    }

    void Cpu::SetRegister(uint32_t regid, uint64_t value) {
        uc_reg_write(uc, regid, &value);
    }

    void Cpu::HookInterrupt(uc_engine *uc, uint32_t intno, void *user_data) {
        if (intno == 2) {
            uint32_t instr{};
            uc_mem_read(uc, GetRegister(UC_ARM64_REG_PC) - 4, &instr, 4);

            uint32_t svcId = instr >> 5 & 0xFF;

            // TODO: Handle SVCs
        } else {
            syslog(LOG_ERR, "Unhandled interrupt #%i", intno);
            uc_close(uc);
        }
    }
}