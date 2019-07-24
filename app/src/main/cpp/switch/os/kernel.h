#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include "switch/common.h"

namespace lightSwitch::os {
    class KObject {
    private:
        uint32_t handle;
    public:
        KObject(uint32_t handle) : handle(handle) {}

        uint32_t Handle() { return handle; }
    };

    typedef std::shared_ptr<KObject> KObjectPtr;

    class Kernel {
    private:
        device_state state;
        uint32_t handle_index = constant::base_handle_index;
        std::unordered_map<uint32_t, KObjectPtr> handles;
    public:
        Kernel(device_state state_);

        uint32_t NewHandle(KObjectPtr obj);
    };
}