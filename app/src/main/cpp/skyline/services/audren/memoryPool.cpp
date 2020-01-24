#include <common.h>
#include "memoryPool.h"

namespace skyline::service::audren {
    void MemoryPool::ProcessInput(const MemoryPoolIn &input) {
        if (input.state == MemoryPoolState::RequestAttach)
            output.state = MemoryPoolState::Attached;
        else if (input.state == MemoryPoolState::RequestDetach)
            output.state = MemoryPoolState::Detached;
    }
}
