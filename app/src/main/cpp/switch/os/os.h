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
    public:
        OS(device_state state_);

        static void SvcHandler(uint16_t svc, void *vstate);

        void HandleSvc(uint16_t svc);
    };
}