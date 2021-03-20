// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::kernel::svc {
    /**
     * @brief Sets the process heap to a given size, it can both extend and shrink the heap
     * @url https://switchbrew.org/wiki/SVC#SetHeapSize
     */
    void SetHeapSize(const DeviceState &state);

    /**
     * @brief Change attribute of page-aligned memory region, this is used to turn on/off caching for a given memory area
     * @url https://switchbrew.org/wiki/SVC#SetMemoryAttribute
     */
    void SetMemoryAttribute(const DeviceState &state);

    /**
     * @brief Maps a memory range into a different range, mainly used for adding guard pages around stack
     * @url https://switchbrew.org/wiki/SVC#SetMemoryAttribute
     */
    void MapMemory(const DeviceState &state);

    /**
     * @brief Unmaps a region that was previously mapped with #MapMemory
     * @url https://switchbrew.org/wiki/SVC#UnmapMemory
     */
    void UnmapMemory(const DeviceState &state);

    /**
     * @brief Query information about an address
     * @url https://switchbrew.org/wiki/SVC#QueryMemory
     */
    void QueryMemory(const DeviceState &state);

    /**
     * @brief Exits the current process
     * @url https://switchbrew.org/wiki/SVC#ExitProcess
     */
    void ExitProcess(const DeviceState &state);

    /**
     * @brief Create a thread in the current process
     * @url https://switchbrew.org/wiki/SVC#CreateThread
     */
    void CreateThread(const DeviceState &state);

    /**
     * @brief Starts the thread for the provided handle
     * @url https://switchbrew.org/wiki/SVC#StartThread
     */
    void StartThread(const DeviceState &state);

    /**
     * @brief Exits the current thread
     * @url https://switchbrew.org/wiki/SVC#ExitThread
     */
    void ExitThread(const DeviceState &state);

    /**
     * @brief Sleep for a specified amount of time, or yield thread
     * @url https://switchbrew.org/wiki/SVC#SleepThread
     */
    void SleepThread(const DeviceState &state);

    /**
     * @brief Get priority of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#GetThreadPriority
     */
    void GetThreadPriority(const DeviceState &state);

    /**
     * @brief Set priority of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#SetThreadPriority
     */
    void SetThreadPriority(const DeviceState &state);

    /**
     * @brief Get core mask of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#GetThreadCoreMask
     */
    void GetThreadCoreMask(const DeviceState &state);

    /**
     * @brief Set core mask of provided thread handle
     * @url https://switchbrew.org/wiki/SVC#SetThreadCoreMask
     */
    void SetThreadCoreMask(const DeviceState &state);

    /**
     * @brief Returns the core on which the current thread is running
     * @url https://switchbrew.org/wiki/SVC#GetCurrentProcessorNumber
     */
    void GetCurrentProcessorNumber(const DeviceState &state);

    /**
     * @brief Resets a KEvent to its unsignalled state
     * @url https://switchbrew.org/wiki/SVC#ClearEvent
     */
    void ClearEvent(const DeviceState &state);

    /**
     * @brief Maps the block supplied by the handle
     * @url https://switchbrew.org/wiki/SVC#MapSharedMemory
     */
    void MapSharedMemory(const DeviceState &state);

    /**
     * @brief Returns a handle to a KSharedMemory object
     * @url https://switchbrew.org/wiki/SVC#CreateTransferMemory
     */
    void CreateTransferMemory(const DeviceState &state);

    /**
     * @brief Closes the specified handle
     * @url https://switchbrew.org/wiki/SVC#CloseHandle
     */
    void CloseHandle(const DeviceState &state);

    /**
     * @brief Resets a particular KEvent or KProcess which is signalled
     * @url https://switchbrew.org/wiki/SVC#ResetSignal
     */
    void ResetSignal(const DeviceState &state);

    /**
     * @brief Stalls a thread till a KSyncObject signals or the timeout has ended
     * @url https://switchbrew.org/wiki/SVC#WaitSynchronization
     */
    void WaitSynchronization(const DeviceState &state);

    /**
     * @brief If the referenced thread is currently in a synchronization call, that call will be interrupted
     * @url https://switchbrew.org/wiki/SVC#CancelSynchronization
     */
    void CancelSynchronization(const DeviceState &state);

    /**
     * @brief Locks a specified mutex
     * @url https://switchbrew.org/wiki/SVC#ArbitrateLock
     */
    void ArbitrateLock(const DeviceState &state);

    /**
     * @brief Unlocks a specified mutex
     * @url https://switchbrew.org/wiki/SVC#ArbitrateUnlock
     */
    void ArbitrateUnlock(const DeviceState &state);

    /**
     * @brief Waits on a process-wide key (Conditional-Variable)
     * @url https://switchbrew.org/wiki/SVC#WaitProcessWideKeyAtomic
     */
    void WaitProcessWideKeyAtomic(const DeviceState &state);

    /**
     * @brief Signals a process-wide key (Conditional-Variable)
     * @url https://switchbrew.org/wiki/SVC#SignalProcessWideKey
     */
    void SignalProcessWideKey(const DeviceState &state);

    /**
     * @brief Returns the value of CNTPCT_EL0 on the Switch
     * @url https://switchbrew.org/wiki/SVC#GetSystemTick
     */
    void GetSystemTick(const DeviceState &state);

    /**
     * @brief Connects to a named IPC port
     * @url https://switchbrew.org/wiki/SVC#ConnectToNamedPort
     */
    void ConnectToNamedPort(const DeviceState &state);

    /**
     * @brief Send a synchronous IPC request to a service
     * @url https://switchbrew.org/wiki/SVC#SendSyncRequest
     */
    void SendSyncRequest(const DeviceState &state);

    /**
     * @brief Retrieves the PID of a specific thread
     * @url https://switchbrew.org/wiki/SVC#GetThreadId
     */
    void GetThreadId(const DeviceState &state);

    /**
     * @brief Causes the debugger to be engaged or the program to end if it isn't being debugged
     * @url https://switchbrew.org/wiki/SVC#Break
     */
    void Break(const DeviceState &state);

    /**
     * @brief Outputs a debug string
     * @url https://switchbrew.org/wiki/SVC#OutputDebugString
     */
    void OutputDebugString(const DeviceState &state);

    /**
     * @brief Retrieves a piece of information
     * @url https://switchbrew.org/wiki/SVC#GetInfo
     */
    void GetInfo(const DeviceState &state);

    /**
     * @brief Maps physical memory to a part of virtual memory
     * @url https://switchbrew.org/wiki/SVC#MapPhysicalMemory
     */
    void MapPhysicalMemory(const DeviceState &state);

    /**
     * @brief Unmaps previously mapped physical memory
     * @url https://switchbrew.org/wiki/SVC#UnmapPhysicalMemory
     */
    void UnmapPhysicalMemory(const DeviceState &state);

    /**
     * @brief Waits on an address based on the value of the address
     * @url https://switchbrew.org/wiki/SVC#WaitForAddress
     */
    void WaitForAddress(const DeviceState &state);

    /**
     * @brief Signals a thread which is waiting on an address
     * @url https://switchbrew.org/wiki/SVC#SignalToAddress
     */
    void SignalToAddress(const DeviceState &state);

    /**
     * @brief A per-SVC descriptor with it's name and a function pointer
     * @note The descriptor is nullable, the validity of the descriptor can be checked with the boolean operator
     */
    struct SvcDescriptor {
        void (*function)(const DeviceState &); //!< A function pointer to a HLE implementation of the SVC
        const char* name; //!< A pointer to a static string of the SVC name, the underlying data should not be mutated

        operator bool() {
            return function;
        }
    };

    #define SVC_NONE SvcDescriptor{} //!< A macro with a placeholder value for the SVC not being implemented or not existing
    #define SVC_STRINGIFY(name) #name
    #define SVC_ENTRY(function) SvcDescriptor{function, SVC_STRINGIFY(Svc ## function)} //!< A macro which automatically stringifies the function name as the name to prevent pointless duplication

    /**
     * @brief The SVC table maps all SVCs to their corresponding functions
     */
    static constexpr std::array<SvcDescriptor, 0x80> SvcTable{
        SVC_NONE, // 0x00 (Does not exist)
        SVC_ENTRY(SetHeapSize), // 0x01
        SVC_NONE, // 0x02
        SVC_ENTRY(SetMemoryAttribute), // 0x03
        SVC_ENTRY(MapMemory), // 0x04
        SVC_ENTRY(UnmapMemory), // 0x05
        SVC_ENTRY(QueryMemory), // 0x06
        SVC_ENTRY(ExitProcess), // 0x07
        SVC_ENTRY(CreateThread), // 0x08
        SVC_ENTRY(StartThread), // 0x09
        SVC_ENTRY(ExitThread), // 0x0A
        SVC_ENTRY(SleepThread), // 0x0B
        SVC_ENTRY(GetThreadPriority), // 0x0C
        SVC_ENTRY(SetThreadPriority), // 0x0D
        SVC_ENTRY(GetThreadCoreMask), // 0x0E
        SVC_ENTRY(SetThreadCoreMask), // 0x0F
        SVC_ENTRY(GetCurrentProcessorNumber), // 0x10
        SVC_NONE, // 0x11
        SVC_ENTRY(ClearEvent), // 0x12
        SVC_ENTRY(MapSharedMemory), // 0x13
        SVC_NONE, // 0x14
        SVC_ENTRY(CreateTransferMemory), // 0x15
        SVC_ENTRY(CloseHandle), // 0x16
        SVC_ENTRY(ResetSignal), // 0x17
        SVC_ENTRY(WaitSynchronization), // 0x18
        SVC_ENTRY(CancelSynchronization), // 0x19
        SVC_ENTRY(ArbitrateLock), // 0x1A
        SVC_ENTRY(ArbitrateUnlock), // 0x1B
        SVC_ENTRY(WaitProcessWideKeyAtomic), // 0x1C
        SVC_ENTRY(SignalProcessWideKey), // 0x1D
        SVC_ENTRY(GetSystemTick), // 0x1E
        SVC_ENTRY(ConnectToNamedPort), // 0x1F
        SVC_NONE, // 0x20
        SVC_ENTRY(SendSyncRequest), // 0x21
        SVC_NONE, // 0x22
        SVC_NONE, // 0x23
        SVC_NONE, // 0x24
        SVC_ENTRY(GetThreadId), // 0x25
        SVC_ENTRY(Break), // 0x26
        SVC_ENTRY(OutputDebugString), // 0x27
        SVC_NONE, // 0x28
        SVC_ENTRY(GetInfo), // 0x29
        SVC_NONE, // 0x2A
        SVC_NONE, // 0x2B
        SVC_ENTRY(MapPhysicalMemory), // 0x2C
        SVC_ENTRY(UnmapPhysicalMemory), // 0x2D
        SVC_NONE, // 0x2E
        SVC_NONE, // 0x2F
        SVC_NONE, // 0x30
        SVC_NONE, // 0x31
        SVC_NONE, // 0x32
        SVC_NONE, // 0x33
        SVC_ENTRY(WaitForAddress), // 0x34
        SVC_ENTRY(SignalToAddress), // 0x35
        SVC_NONE, // 0x36
        SVC_NONE, // 0x37
        SVC_NONE, // 0x38
        SVC_NONE, // 0x39
        SVC_NONE, // 0x3A
        SVC_NONE, // 0x3B
        SVC_NONE, // 0x3C
        SVC_NONE, // 0x3D
        SVC_NONE, // 0x3E
        SVC_NONE, // 0x3F
        SVC_NONE, // 0x40
        SVC_NONE, // 0x41
        SVC_NONE, // 0x42
        SVC_NONE, // 0x43
        SVC_NONE, // 0x44
        SVC_NONE, // 0x45
        SVC_NONE, // 0x46
        SVC_NONE, // 0x47
        SVC_NONE, // 0x48
        SVC_NONE, // 0x49
        SVC_NONE, // 0x4A
        SVC_NONE, // 0x4B
        SVC_NONE, // 0x4C
        SVC_NONE, // 0x4D
        SVC_NONE, // 0x4E
        SVC_NONE, // 0x4F
        SVC_NONE, // 0x50
        SVC_NONE, // 0x51
        SVC_NONE, // 0x52
        SVC_NONE, // 0x53
        SVC_NONE, // 0x54
        SVC_NONE, // 0x55
        SVC_NONE, // 0x56
        SVC_NONE, // 0x57
        SVC_NONE, // 0x58
        SVC_NONE, // 0x59
        SVC_NONE, // 0x5A
        SVC_NONE, // 0x5B
        SVC_NONE, // 0x5C
        SVC_NONE, // 0x5D
        SVC_NONE, // 0x5E
        SVC_NONE, // 0x5F
        SVC_NONE, // 0x60
        SVC_NONE, // 0x61
        SVC_NONE, // 0x62
        SVC_NONE, // 0x63
        SVC_NONE, // 0x64
        SVC_NONE, // 0x65
        SVC_NONE, // 0x66
        SVC_NONE, // 0x67
        SVC_NONE, // 0x68
        SVC_NONE, // 0x69
        SVC_NONE, // 0x6A
        SVC_NONE, // 0x6B
        SVC_NONE, // 0x6C
        SVC_NONE, // 0x6D
        SVC_NONE, // 0x6E
        SVC_NONE, // 0x6F
        SVC_NONE, // 0x70
        SVC_NONE, // 0x71
        SVC_NONE, // 0x72
        SVC_NONE, // 0x73
        SVC_NONE, // 0x74
        SVC_NONE, // 0x75
        SVC_NONE, // 0x76
        SVC_NONE, // 0x77
        SVC_NONE, // 0x78
        SVC_NONE, // 0x79
        SVC_NONE, // 0x7A
        SVC_NONE, // 0x7B
        SVC_NONE, // 0x7C
        SVC_NONE, // 0x7D
        SVC_NONE, // 0x7E
        SVC_NONE // 0x7F
    };

    #undef SVC_NONE
    #undef SVC_STRINGIFY
    #undef SVC_ENTRY
}
