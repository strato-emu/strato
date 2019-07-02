#include <sys/mman.h>
#include "cpu.h"
#include "memory.h"

// TODO: Handle Unicorn errors
namespace core::cpu {
    uc_engine *uc;

    void HookInterrupt(uc_engine *uc, uint32_t intno, void *user_data);

    bool Initialize() {
        uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc);

        uc_hook hook{};
        uc_hook_add(uc, &hook, UC_HOOK_INTR, (void *) HookInterrupt, 0, 1, 0);

        // Map stack memory
        if (!memory::Map(0x3000000, 0x1000000, "stack")) return false;
        SetRegister(UC_ARM64_REG_SP, 0x3100000);
        // Map TLS memory
        if (!memory::Map(0x2000000, 0x1000, "tls")) return false;
        SetRegister(UC_ARM64_REG_TPIDRRO_EL0, 0x2000000);

        return true;
    }

    void Run(uint64_t address) {
        uc_err err = uc_emu_start(uc, address, 1ULL << 63, 0, 0);
        if (err)
            syslog(LOG_ERR, "uc_emu_start failed: %s", uc_strerror(err));
    }

    uint64_t GetRegister(uint32_t regid) {
        uint64_t registerValue;
        uc_reg_read(uc, regid, &registerValue);
        return registerValue;
    }

    void SetRegister(uint32_t regid, uint64_t value) {
        uc_reg_write(uc, regid, &value);
    }

    void HookInterrupt(uc_engine *uc, uint32_t intno, void *user_data) {
        if (intno == 2) {
            uint32_t instr{};
            uc_mem_read(uc, GetRegister(UC_ARM64_REG_PC) - 4, &instr, 4);

            uint32_t svcId = instr >> 5 & 0xFF;

            // TODO: Handle SVCs
            syslog(LOG_DEBUG, "SVC 0x%x called!", svcId);
            uc_close(uc);
        } else {
            syslog(LOG_ERR, "Unhandled interrupt #%i", intno);
            uc_close(uc);
        }
    }
}

// FIXME: Move this back to memory.cpp - zephyren25
namespace core::memory {
    bool Map(uint64_t address, size_t size, std::string label) {
        void *ptr = mmap((void*)(address), size, PROT_EXEC | PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, 0, 0);
        if (!ptr) {
            syslog(LOG_ERR, "Failed mapping region '%s'", label.c_str());
            return false;
        }

        uc_err err = uc_mem_map_ptr(core::cpu::uc, address, size, UC_PROT_ALL, (void*)(address));
        if (err) {
            syslog(LOG_ERR, "UC map failed: %s", uc_strerror(err));
            return false;
        }

        syslog(LOG_DEBUG, "Successfully mapped region '%s' to 0x%x", label.c_str(), address);
        return true;
    }
}