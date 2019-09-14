#pragma once

#include "KObject.h"

namespace lightSwitch::kernel::type {
    /**
     * KThread class is responsible for holding the state of a thread
     */
    class KThread : public KObject {
      private:
        KProcess *parent; //!< The parent process of this thread
        const device_state &state; //!< The state of the device
        u64 entry_point; //!< The address to start execution at
        u64 entry_arg; //!< An argument to pass to the process on entry

      public:
        handle_t handle; //!< The handle of the current thread in it's parent process's handle table
        pid_t pid; //!< The PID of the current thread (As in kernel PID and not PGID [In short, Linux implements threads as processes that share a lot of stuff at the kernel level])
        u64 stack_top; //!< The top of the stack (Where it starts growing downwards from)
        u64 tls; //!< The address of TLS (Thread Local Storage) slot assigned to the current thread
        u8 priority; //!< Hold the priority of a thread in Nintendo format

        /**
         * @param handle The handle of the current thread
         * @param pid The PID of the current thread
         * @param entry_point The address to start execution at
         * @param entry_arg An argument to pass to the process on entry
         * @param stack_top The top of the stack
         * @param tls The address of the TLS slot assigned
         * @param priority The priority of the thread in Nintendo format
         * @param parent The parent process of this thread
         * @param arg An optional argument to pass to the process
         */
        KThread(handle_t handle, pid_t pid, u64 entry_point, u64 entry_arg, u64 stack_top, u64 tls, u8 priority, KProcess *parent, const device_state &state);

        /**
         * Kills the thread and deallocates the memory allocated for stack.
         */
        ~KThread();

        /**
         * Starts the current thread
         */
        void Start();

        /**
         * Set the priority of the current thread to `priority` using setpriority [https://linux.die.net/man/3/setpriority].
         * We rescale the priority from Nintendo scale to that of Android.
         * @param priority The priority of the thread in Nintendo format
         */
        void UpdatePriority(u8 priority);
    };
}
