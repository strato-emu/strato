#include "kernel.h"
#include "svc.h"

namespace lightSwitch::os {
    Kernel::Kernel(device_state state_) : state(state_) {}

    uint32_t Kernel::NewHandle(KObjectPtr obj) {
        handles.insert({handle_index, obj});
        state.logger->write(Logger::DEBUG, "Creating new handle 0x{0:x}", handle_index);
        return handle_index++;
    }
}