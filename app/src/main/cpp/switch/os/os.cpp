
#include <unicorn/arm64.h>
#include "os.h"

namespace lightSwitch::os {
    OS::OS(device_state state_) : state(std::move(state_)) {
        uc_err err = uc_hook_add(state.cpu->GetEngine(), &hook, UC_HOOK_INTR,
                                 (void *) HookInterrupt, &state, 1, 0);
        if (err)
            throw std::runtime_error("An error occurred while running 'uc_hook_add': " +
                                     std::string(uc_strerror(err)));
    }

    OS::~OS() {
        uc_hook_del(state.cpu->GetEngine(), hook);
    }

    void OS::HookInterrupt(uc_engine *uc, uint32_t int_no, void *user_data) {
        device_state state = *((device_state *) user_data);
        try {
            if (int_no == 2) {
                uint32_t instr{};
                uc_err err = uc_mem_read(uc, state.cpu->GetRegister(UC_ARM64_REG_PC) - 4, &instr, 4);
                if (err)
                    throw exception("An error occurred while running 'uc_mem_read': " + std::string(uc_strerror(err)));
                uint32_t svcId = instr >> 5U & 0xFF;
                SvcHandler(svcId, state);
            } else {
                state.logger->write(Logger::ERROR, "An unhandled interrupt has occurred: {}", int_no);
                exit(int_no);
            }
        } catch (exception &e) {
            state.logger->write(Logger::WARN, "An exception occurred during an interrupt: {}", e.what());
        } catch (...) {
            state.logger->write(Logger::WARN, "An unknown exception has occurred.");
        }
    }

    void OS::SvcHandler(uint32_t svc, device_state &state) {
        if (svc::svcTable[svc])
            (*svc::svcTable[svc])(state);
        else
            state.logger->write(Logger::WARN, "Unimplemented SVC 0x{0:x}", svc);
    }

    void OS::SvcHandler(uint32_t svc) {
        SvcHandler(svc, state);
    }
}