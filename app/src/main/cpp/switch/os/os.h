#pragma once

#include <cstdint>
#include <err.h>
#include "switch/common.h"
#include "ipc.h"
#include "kernel.h"
#include "svc.h"

namespace lightSwitch::os {
    class OS {
    private:
        device_state state;
        uc_hook hook{};
    public:
        OS(device_state state_);

        ~OS();

        static void HookInterrupt(uc_engine *uc, uint32_t int_no, void *user_data);

        static void SvcHandler(uint32_t svc, device_state &state);

        void SvcHandler(uint32_t svc);
    };
}