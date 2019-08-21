
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

    uint32_t OS::NewHandle(KObjectPtr obj) {
        handles.insert({handle_index, obj});
        state.logger->write(Logger::DEBUG, "Creating new handle 0x{0:x}", handle_index);
        return handle_index++;
    }
}