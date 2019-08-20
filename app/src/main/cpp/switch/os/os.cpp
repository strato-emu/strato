
#include "os.h"

namespace lightSwitch::os {
    OS::OS(device_state state_) : state(std::move(state_)) {}

    void OS::SvcHandler(uint16_t svc, void *vstate) {
        device_state state = *((device_state *) vstate);
        if (svc::svcTable[svc])
            (*svc::svcTable[svc])(state);
        else {
            state.logger->write(Logger::ERROR, "Unimplemented SVC 0x{0:X}", svc);
            state.cpu->StopExecution();
        }
    }

    void OS::HandleSvc(uint16_t svc) {
        SvcHandler(svc, &state);
    }
}