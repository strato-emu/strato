#pragma once
#include <cstdint>
#include <memory>

#define SM_HANDLE 0xd000 // sm: is hardcoded for now

namespace core::kernel
{
    class KObject
    {
    public:
        KObject(uint32_t handle) : handle(handle) {}
        uint32_t Handle() { return handle; }
    private:
        uint32_t handle;
    };

    typedef std::shared_ptr<KObject> KObjectPtr;

    uint32_t NewHandle(KObjectPtr obj);
}