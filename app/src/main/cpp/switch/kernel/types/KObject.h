#pragma once

#include "../../common.h"

namespace lightSwitch::kernel::type {
    enum class KObjectType {
        KThread, KProcess
    };
    class KObject {
    public:
        uint32_t handle;
        KObjectType type;
        KObject(handle_t handle, KObjectType type) : handle(handle), type(type) {}
    };
}
