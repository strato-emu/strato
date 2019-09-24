#pragma once

#include "../../common.h"

namespace skyline::kernel::type {
    /**
     * @brief These types are used to perform runtime evaluation of a kernel object's type when converting from base class
     */
    enum class KType {
        KThread, KProcess, KSharedMemory, KPrivateMemory, KSession
    };

    /**
     * @brief A base class that all Kernel objects have to derive from
     */
    class KObject {
      public:
        handle_t handle; //!< The handle of this KObject
        pid_t ownerPid; //!< The pid of the process owning this object
        const DeviceState &state; //!< The state of the device
        KType handleType; //!< The type of this object

        /**
         * @param handle The handle of the object in the handle table
         * @param ownerPid The PID of the process which owns this
         * @param state The state of the device
         * @param handleType The type of the object
         */
        KObject(handle_t handle, pid_t ownerPid, const DeviceState &state, KType handleType) : handle(handle), ownerPid(ownerPid), state(state), handleType(handleType) {}
    };
}
