#pragma once

#include "KSyncObject.h"
#include "KSharedMemory.h"

namespace skyline::kernel::type {
    /**
     * @brief KThread class is responsible for holding the state of a thread
     */
    class KThread : public KSyncObject {
      private:
        KProcess *parent; //!< The parent process of this thread
        u64 entryPoint; //!< The address to start execution at
        u64 entryArg; //!< An argument to pass to the process on entry

      public:
        enum class Status {
            Created, //!< The thread has been created but has not been started yet
            Running, //!< The thread is running currently
            Dead //!< The thread is dead and not running
        } status = Status::Created; //!< The state of the thread
        std::shared_ptr<type::KSharedMemory> ctxMemory; //!< The KSharedMemory of the shared memory allocated by the guest process TLS
        handle_t handle; // The handle of the object in the handle table
        pid_t pid; //!< The PID of the current thread (As in kernel PID and not PGID [In short, Linux implements threads as processes that share a lot of stuff at the kernel level])
        u64 stackTop; //!< The top of the stack (Where it starts growing downwards from)
        u64 tls; //!< The address of TLS (Thread Local Storage) slot assigned to the current thread
        u8 priority; //!< Hold the priority of a thread in Nintendo format

        /**
         * @param state The state of the device
         * @param handle The handle of the current thread
         * @param self_pid The PID of this thread
         * @param entryPoint The address to start execution at
         * @param entryArg An argument to pass to the process on entry
         * @param stackTop The top of the stack
         * @param tls The address of the TLS slot assigned
         * @param priority The priority of the thread in Nintendo format
         * @param parent The parent process of this thread
         * @param tlsMemory The KSharedMemory object for TLS memory allocated by the guest process
         */
        KThread(const DeviceState &state, handle_t handle, pid_t self_pid, u64 entryPoint, u64 entryArg, u64 stackTop, u64 tls, u8 priority, KProcess *parent, std::shared_ptr<type::KSharedMemory> &tlsMemory);

        /**
         * @brief Kills the thread and deallocates the memory allocated for stack.
         */
        ~KThread();

        /**
         * @brief This starts this thread process
         */
        void Start();

        /**
         * @brief This kills the thread
         */
        void Kill();

        /**
         * @brief This wakes up the thread from it's sleep (no-op if thread is already awake)
         */
        void WakeUp();

        /**
         * @brief Update the priority level for the process.
         * @details Set the priority of the current thread to `priority` using setpriority [https://linux.die.net/man/3/setpriority]. We rescale the priority from Nintendo scale to that of Android.
         * @param priority The priority of the thread in Nintendo format
         */
        void UpdatePriority(u8 priority);
    };
}
