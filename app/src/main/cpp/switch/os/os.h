#pragma once

#include <unordered_map>
#include "../common.h"
#include "ipc.h"
#include "svc.h"

namespace lightSwitch::os {
    class KObject {
    private:
        uint32_t handle;
    public:
        KObject(uint32_t handle) : handle(handle) {}

        uint32_t Handle() { return handle; }
    };

    typedef std::shared_ptr<KObject> KObjectPtr;

    class OS {
    private:
        device_state state;
        uint32_t handle_index = constant::base_handle_index;
        std::unordered_map<uint32_t, KObjectPtr> handles;
    public:
        OS(device_state state_);

        void SvcHandler(uint16_t svc, void *vstate);

        uint32_t NewHandle(KObjectPtr obj);
    };
}