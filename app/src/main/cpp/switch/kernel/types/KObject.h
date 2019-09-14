#pragma once

#include "../../common.h"

namespace lightSwitch::kernel::type {
    enum class KObjectType {
        KThread, KProcess, KSharedMemory
    };

    class KObject {
      public:
        u32 handle;
        KObjectType type;

        KObject(handle_t handle, KObjectType type) : handle(handle), type(type) {}
    };
}
