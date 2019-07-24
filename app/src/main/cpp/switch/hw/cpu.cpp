#include "cpu.h"
#include "../constant.h"


namespace lightSwitch::hw {
    Cpu::Cpu() {
        err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc);
        if (err)
            throw std::runtime_error("An error occurred while running 'uc_open': " + std::string(uc_strerror(err)));
    }

    Cpu::~Cpu() {
        uc_close(uc);
    }

    void Cpu::Execute(uint64_t address) {
        // Set Registers
        SetRegister(UC_ARM64_REG_SP, constant::stack_addr + 0x100000); // Stack Pointer (For some reason programs move the stack pointer backwards so 0x100000 is added)
        SetRegister(UC_ARM64_REG_TPIDRRO_EL0, constant::tls_addr); // User Read-Only Thread ID Register
        err = uc_emu_start(uc, address, std::numeric_limits<uint64_t>::max(), 0, 0);
        if (err)
            throw std::runtime_error("An error occurred while running 'uc_emu_start': " + std::string(uc_strerror(err)));
    }

    void Cpu::StopExecution() {
        uc_emu_stop(uc);
    }

    uint64_t Cpu::GetRegister(uint32_t reg_id) {
        uint64_t registerValue;
        err = uc_reg_read(uc, reg_id, &registerValue);
        if (err)
            throw std::runtime_error("An error occurred while running 'uc_reg_read': " + std::string(uc_strerror(err)));
        return registerValue;
    }

    void Cpu::SetRegister(uint32_t regid, uint64_t value) {
        err = uc_reg_write(uc, regid, &value);
        if (err)
            throw std::runtime_error("An error occurred while running 'uc_reg_write': " + std::string(uc_strerror(err)));
    }

    uc_engine *Cpu::GetEngine() {
        return uc;
    }
}