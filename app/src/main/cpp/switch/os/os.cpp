
#include "os.h"

namespace lightSwitch::os {
    OS::OS(device_state state_) : state(std::move(state_)) {}

    void OS::SvcHandler(uint16_t svc, void *vstate) {
        device_state state = *((device_state *) vstate);
        if (svc::svcTable[svc])
            (*svc::svcTable[svc])(state);
        else
            state.logger->write(Logger::WARN, "Unimplemented SVC 0x{0:x}", svc);
    }

    void OS::HandleSvc(uint16_t svc) {
        SvcHandler(svc, &state);
    }
}