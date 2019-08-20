#pragma once

#include <syslog.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/uio.h>
#include <linux/elf.h>
#include <switch/constant.h>
#include "memory.h"

namespace lightSwitch::hw {
    class Cpu {
    private:
        bool halt = false;
        long status = 0;
        pid_t child;
        iovec iov;
        user_pt_regs regs;
        uint64_t tls;

        static pid_t ExecuteChild(uint64_t address);

        void ReadRegisters();

        void WriteRegisters();

        long *ReadMemory(uint64_t address);

        void WriteMemory(uint64_t address);

        void ResumeProcess();

        void WriteBreakpoint(uint64_t address, uint64_t size);

        uint8_t wreg_lut[31] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60};

    public:
        ~Cpu();

        void Execute(Memory::Region region, std::shared_ptr<Memory> memory, std::function<void(uint16_t, void *)> svc_handler, void *device);

        void StopExecution();

        uint64_t GetRegister(xreg reg_id);

        void SetRegister(xreg reg_id, uint64_t value);

        uint64_t GetRegister(wreg reg_id);

        void SetRegister(wreg reg_id, uint32_t value);
    };
}
