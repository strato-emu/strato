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
     * @brief Clears a KEvent of it's signal
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
     * @brief The SVC Table maps all SVCs to their corresponding functions
     */
    static std::array<void (*)(const DeviceState &), 0x80> SvcTable{
        nullptr, // 0x00 (Does not exist)
        SetHeapSize, // 0x01
        nullptr, // 0x02
        SetMemoryAttribute, // 0x03
        MapMemory, // 0x04
        UnmapMemory, // 0x05
        QueryMemory, // 0x06
        ExitProcess, // 0x07
        CreateThread, // 0x08
        StartThread, // 0x09
        ExitThread, // 0x0A
        SleepThread, // 0x0B
        GetThreadPriority, // 0x0C
        SetThreadPriority, // 0x0D
        GetThreadCoreMask, // 0x0E
        SetThreadCoreMask, // 0x0F
        GetCurrentProcessorNumber, // 0x10
        nullptr, // 0x11
        ClearEvent, // 0x12
        MapSharedMemory, // 0x13
        nullptr, // 0x14
        CreateTransferMemory, // 0x15
        CloseHandle, // 0x16
        ResetSignal, // 0x17
        WaitSynchronization, // 0x18
        CancelSynchronization, // 0x19
        ArbitrateLock, // 0x1A
        ArbitrateUnlock, // 0x1B
        WaitProcessWideKeyAtomic, // 0x1C
        SignalProcessWideKey, // 0x1D
        GetSystemTick, // 0x1E
        ConnectToNamedPort, // 0x1F
        nullptr, // 0x20
        SendSyncRequest, // 0x21
        nullptr, // 0x22
        nullptr, // 0x23
        nullptr, // 0x24
        GetThreadId, // 0x25
        nullptr, // 0x26
        OutputDebugString, // 0x27
        nullptr, // 0x28
        GetInfo, // 0x29
        nullptr, // 0x2A
        nullptr, // 0x2B
        MapPhysicalMemory, // 0x2C
        UnmapPhysicalMemory, // 0x2D
        nullptr, // 0x2E
        nullptr, // 0x2F
        nullptr, // 0x30
        nullptr, // 0x31
        nullptr, // 0x32
        nullptr, // 0x33
        nullptr, // 0x34
        nullptr, // 0x35
        nullptr, // 0x36
        nullptr, // 0x37
        nullptr, // 0x38
        nullptr, // 0x39
        nullptr, // 0x3A
        nullptr, // 0x3B
        nullptr, // 0x3C
        nullptr, // 0x3D
        nullptr, // 0x3E
        nullptr, // 0x3F
        nullptr, // 0x40
        nullptr, // 0x41
        nullptr, // 0x42
        nullptr, // 0x43
        nullptr, // 0x44
        nullptr, // 0x45
        nullptr, // 0x46
        nullptr, // 0x47
        nullptr, // 0x48
        nullptr, // 0x49
        nullptr, // 0x4A
        nullptr, // 0x4B
        nullptr, // 0x4C
        nullptr, // 0x4D
        nullptr, // 0x4E
        nullptr, // 0x4F
        nullptr, // 0x50
        nullptr, // 0x51
        nullptr, // 0x52
        nullptr, // 0x53
        nullptr, // 0x54
        nullptr, // 0x55
        nullptr, // 0x56
        nullptr, // 0x57
        nullptr, // 0x58
        nullptr, // 0x59
        nullptr, // 0x5A
        nullptr, // 0x5B
        nullptr, // 0x5C
        nullptr, // 0x5D
        nullptr, // 0x5E
        nullptr, // 0x5F
        nullptr, // 0x60
        nullptr, // 0x61
        nullptr, // 0x62
        nullptr, // 0x63
        nullptr, // 0x64
        nullptr, // 0x65
        nullptr, // 0x66
        nullptr, // 0x67
        nullptr, // 0x68
        nullptr, // 0x69
        nullptr, // 0x6A
        nullptr, // 0x6B
        nullptr, // 0x6C
        nullptr, // 0x6D
        nullptr, // 0x6E
        nullptr, // 0x6F
        nullptr, // 0x70
        nullptr, // 0x71
        nullptr, // 0x72
        nullptr, // 0x73
        nullptr, // 0x74
        nullptr, // 0x75
        nullptr, // 0x76
        nullptr, // 0x77
        nullptr, // 0x78
        nullptr, // 0x79
        nullptr, // 0x7A
        nullptr, // 0x7B
        nullptr, // 0x7C
        nullptr, // 0x7D
        nullptr, // 0x7E
        nullptr // 0x7F
    };
}
