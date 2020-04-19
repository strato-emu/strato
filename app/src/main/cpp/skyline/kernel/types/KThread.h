// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

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
        std::atomic<bool> cancelSync{false}; //!< This is to flag to a thread to cancel a synchronization call it currently is in
        std::shared_ptr<type::KSharedMemory> ctxMemory; //!< The KSharedMemory of the shared memory allocated by the guest process TLS
        KHandle handle; // The handle of the object in the handle table
        pid_t tid; //!< The TID of the current thread
        u64 stackTop; //!< The top of the stack (Where it starts growing downwards from)
        u64 tls; //!< The address of TLS (Thread Local Storage) slot assigned to the current thread
        u8 priority; //!< The priority of a thread in Nintendo format

        /**
         * @param state The state of the device
         * @param handle The handle of the current thread
         * @param selfTid The TID of this thread
         * @param entryPoint The address to start execution at
         * @param entryArg An argument to pass to the process on entry
         * @param stackTop The top of the stack
         * @param tls The address of the TLS slot assigned
         * @param priority The priority of the thread in Nintendo format
         * @param parent The parent process of this thread
         * @param tlsMemory The KSharedMemory object for TLS memory allocated by the guest process
         */
        KThread(const DeviceState &state, KHandle handle, pid_t selfTid, u64 entryPoint, u64 entryArg, u64 stackTop, u64 tls, u8 priority, KProcess *parent, std::shared_ptr<type::KSharedMemory> &tlsMemory);

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
         * @brief Update the priority level for the process.
         * @details Set the priority of the current thread to `priority` using setpriority [https://linux.die.net/man/3/setpriority]. We rescale the priority from Nintendo scale to that of Android.
         * @param priority The priority of the thread in Nintendo format
         */
        void UpdatePriority(u8 priority);
    };
}
