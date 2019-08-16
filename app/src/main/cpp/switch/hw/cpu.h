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
        bool stop = false;
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

        void WriteBreakpoint(uint64_t &address, uint64_t &size);

        uint8_t wreg_lut[31] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61};

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
