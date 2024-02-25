// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "svc_context.h"

namespace skyline::kernel::svc {
    /**
     * @brief Sets the process heap to a given size, it can both extend and shrink the heap
     * @url https://switchbrew.org/wiki/SVC#SetHeapSize
     */
    void SetHeapSize(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Reprotects a page-aligned memory region.
     * @url https://switchbrew.org/wiki/SVC#SetMemoryPermission
     */
    void SetMemoryPermission(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Change attribute of page-aligned memory region, this is used to turn on/off caching for a given memory area
     * @url https://switchbrew.org/wiki/SVC#SetMemoryAttribute
     */
    void SetMemoryAttribute(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Maps a memory range into a different range, mainly used for adding guard pages around stack
     * @url https://switchbrew.org/wiki/SVC#MapMemory
     */
    void MapMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Unmaps a region that was previously mapped with #MapMemory
     * @url https://switchbrew.org/wiki/SVC#UnmapMemory
     */
    void UnmapMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Query information about an address
     * @url https://switchbrew.org/wiki/SVC#QueryMemory
     */
    void QueryMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Exits the current process
     * @url https://switchbrew.org/wiki/SVC#ExitProcess
     */
    void ExitProcess(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Create a thread in the current process
     * @url https://switchbrew.org/wiki/SVC#CreateThread
     */
    void CreateThread(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Starts the thread for the provided handle
     * @url https://switchbrew.org/wiki/SVC#StartThread
     */
    void StartThread(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Exits the current thread
     * @url https://switchbrew.org/wiki/SVC#ExitThread
     */
    void ExitThread(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Sleep for a specified amount of time, or yield thread
     * @url https://switchbrew.org/wiki/SVC#SleepThread
     */
    void SleepThread(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Get priority of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#GetThreadPriority
     */
    void GetThreadPriority(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Set priority of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#SetThreadPriority
     */
    void SetThreadPriority(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Get core mask of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#GetThreadCoreMask
     */
    void GetThreadCoreMask(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Set core mask of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#SetThreadCoreMask
     */
    void SetThreadCoreMask(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Returns the core on which the current thread is running
     * @url https://switchbrew.org/wiki/SVC#GetCurrentProcessorNumber
     */
    void GetCurrentProcessorNumber(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Resets a KEvent to its unsignalled state
     * @url https://switchbrew.org/wiki/SVC#ClearEvent
     */
    void ClearEvent(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Maps shared memory into a memory region
     * @url https://switchbrew.org/wiki/SVC#MapSharedMemory
     */
    void MapSharedMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Unmaps shared memory which has been mapped
     * @url https://switchbrew.org/wiki/SVC#UnmapSharedMemory
     */
    void UnmapSharedMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Returns a handle to a KTransferMemory object
     * @url https://switchbrew.org/wiki/SVC#CreateTransferMemory
     */
    void CreateTransferMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Closes the specified handle
     * @url https://switchbrew.org/wiki/SVC#CloseHandle
     */
    void CloseHandle(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Resets a particular KEvent or KProcess which is signalled
     * @url https://switchbrew.org/wiki/SVC#ResetSignal
     */
    void ResetSignal(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Stalls a thread till a KSyncObject signals or the timeout has ended
     * @url https://switchbrew.org/wiki/SVC#WaitSynchronization
     */
    void WaitSynchronization(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief If the referenced thread is currently in a synchronization call, that call will be interrupted
     * @url https://switchbrew.org/wiki/SVC#CancelSynchronization
     */
    void CancelSynchronization(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Locks a specified mutex
     * @url https://switchbrew.org/wiki/SVC#ArbitrateLock
     */
    void ArbitrateLock(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Unlocks a specified mutex
     * @url https://switchbrew.org/wiki/SVC#ArbitrateUnlock
     */
    void ArbitrateUnlock(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Waits on a process-wide key (Conditional-Variable)
     * @url https://switchbrew.org/wiki/SVC#WaitProcessWideKeyAtomic
     */
    void WaitProcessWideKeyAtomic(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Signals a process-wide key (Conditional-Variable)
     * @url https://switchbrew.org/wiki/SVC#SignalProcessWideKey
     */
    void SignalProcessWideKey(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Returns the value of CNTPCT_EL0 on the Switch
     * @url https://switchbrew.org/wiki/SVC#GetSystemTick
     */
    void GetSystemTick(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Connects to a named IPC port
     * @url https://switchbrew.org/wiki/SVC#ConnectToNamedPort
     */
    void ConnectToNamedPort(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Send a synchronous IPC request to a service
     * @url https://switchbrew.org/wiki/SVC#SendSyncRequest
     */
    void SendSyncRequest(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Retrieves the PID of a specific thread
     * @url https://switchbrew.org/wiki/SVC#GetThreadId
     */
    void GetThreadId(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Causes the debugger to be engaged or the program to end if it isn't being debugged
     * @url https://switchbrew.org/wiki/SVC#Break
     */
    void Break(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Outputs a debug string
     * @url https://switchbrew.org/wiki/SVC#OutputDebugString
     */
    void OutputDebugString(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Retrieves a piece of information
     * @url https://switchbrew.org/wiki/SVC#GetInfo
     */
    void GetInfo(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Maps physical memory to a part of virtual memory
     * @url https://switchbrew.org/wiki/SVC#MapPhysicalMemory
     */
    void MapPhysicalMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Unmaps previously mapped physical memory
     * @url https://switchbrew.org/wiki/SVC#UnmapPhysicalMemory
     */
    void UnmapPhysicalMemory(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Sets if a thread is runnable or paused
     * @url https://switchbrew.org/wiki/SVC#SetThreadActivity
     */
    void SetThreadActivity(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Gets the context structure of a paused thread
     * @url https://switchbrew.org/wiki/SVC#GetThreadContext3
     */
    void GetThreadContext3(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Waits on an address based on the value of the address
     * @url https://switchbrew.org/wiki/SVC#WaitForAddress
     */
    void WaitForAddress(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief Signals a thread which is waiting on an address
     * @url https://switchbrew.org/wiki/SVC#SignalToAddress
     */
    void SignalToAddress(const DeviceState &state, SvcContext &ctx);

    /**
     * @brief A per-SVC descriptor with its name and a function pointer
     * @note The descriptor is nullable, the validity of the descriptor can be checked with the boolean operator
     */
    struct SvcDescriptor {
        void (*function)(const DeviceState &, SvcContext &); //!< A function pointer to a HLE implementation of the SVC
        const char *name; //!< A pointer to a static string of the SVC name, the underlying data should not be mutated

        operator bool() {
            return function;
        }
    };

    /**
     * @brief The SVC table that maps all SVCs to their corresponding functions
     */
    extern const std::array<SvcDescriptor, 0x80> SvcTable;
}
