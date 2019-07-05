#include <unordered_map>
#include <syslog.h>
#include "kernel.h"

namespace core::kernel
{
    std::unordered_map<uint32_t, KObjectPtr> handles;
    uint32_t handleIndex = 0xd001;

    uint32_t NewHandle(KObjectPtr obj)
    {
        handles.insert({handleIndex, obj});
        syslog(LOG_DEBUG, "Creating new handle 0x%x", handleIndex);
        return handleIndex++;
    }
}