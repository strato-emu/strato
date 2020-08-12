// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "ipc.h"

namespace skyline {
    namespace constant::infoState {
        // 1.0.0+
        constexpr u8 AllowedCpuIdBitmask = 0x0;
        constexpr u8 AllowedThreadPriorityMask = 0x1;
        constexpr u8 AliasRegionBaseAddr = 0x2;
        constexpr u8 AliasRegionSize = 0x3;
        constexpr u8 HeapRegionBaseAddr = 0x4;
        constexpr u8 HeapRegionSize = 0x5;
        constexpr u8 TotalMemoryAvailable = 0x6;
        constexpr u8 TotalMemoryUsage = 0x7;
        constexpr u8 IsCurrentProcessBeingDebugged = 0x8;
        constexpr u8 ResourceLimit = 0x9;
        constexpr u8 IdleTickCount = 0xA;
        constexpr u8 RandomEntropy = 0xB;
        // 2.0.0+
        constexpr u8 AddressSpaceBaseAddr = 0xC;
        constexpr u8 AddressSpaceSize = 0xD;
        constexpr u8 StackRegionBaseAddr = 0xE;
        constexpr u8 StackRegionSize = 0xF;
        // 3.0.0+
        constexpr u8 PersonalMmHeapSize = 0x10;
        constexpr u8 PersonalMmHeapUsage = 0x11;
        constexpr u8 TitleId = 0x12;
        // 4.0.0+
        constexpr u8 PrivilegedProcessId = 0x13;
        // 5.0.0+
        constexpr u8 UserExceptionContextAddr = 0x14;
        // 6.0.0+
        constexpr u8 TotalMemoryAvailableWithoutMmHeap = 0x15;
        constexpr u8 TotalMemoryUsedWithoutMmHeap = 0x16;
    }

    namespace kernel::svc {
        /**
         * @brief Sets the process heap to a given Size. It can both extend and shrink the heap. (https://switchbrew.org/wiki/SVC#SetHeapSize)
         */
        void SetHeapSize(DeviceState &state);

        /**
         * @brief Change attribute of page-aligned memory region. This is used to turn on/off caching for a given memory area. (https://switchbrew.org/wiki/SVC#SetMemoryAttribute)
         */
        void SetMemoryAttribute(DeviceState &state);

        /**
         * @brief Maps a memory range into a different range. Mainly used for adding guard pages around stack. (https://switchbrew.org/wiki/SVC#SetMemoryAttribute)
         */
        void MapMemory(DeviceState &state);

        /**
         * @brief Unmaps a region that was previously mapped with #MapMemory. (https://switchbrew.org/wiki/SVC#UnmapMemory)
         */
        void UnmapMemory(DeviceState &state);

        /**
         * @brief Query information about an address (https://switchbrew.org/wiki/SVC#QueryMemory)
         */
        void QueryMemory(DeviceState &state);

        /**
         * @brief Exits the current process (https://switchbrew.org/wiki/SVC#ExitProcess)
         */
        void ExitProcess(DeviceState &state);

        /**
         * @brief Create a thread in the current process (https://switchbrew.org/wiki/SVC#CreateThread)
         */
        void CreateThread(DeviceState &state);

        /**
         * @brief Starts the thread for the provided handle (https://switchbrew.org/wiki/SVC#StartThread)
         */
        void StartThread(DeviceState &state);

        /**
         * @brief Exits the current thread (https://switchbrew.org/wiki/SVC#ExitThread)
         */
        void ExitThread(DeviceState &state);

        /**
         * @brief Sleep for a specified amount of time, or yield thread (https://switchbrew.org/wiki/SVC#SleepThread)
         */
        void SleepThread(DeviceState &state);

        /**
         * @brief Get priority of provided thread handle (https://switchbrew.org/wiki/SVC#GetThreadPriority)
         */
        void GetThreadPriority(DeviceState &state);

        /**
         * @brief Set priority of provided thread handle (https://switchbrew.org/wiki/SVC#SetThreadPriority)
         */
        void SetThreadPriority(DeviceState &state);

        /**
         * @brief Clears a KEvent of it's signal (https://switchbrew.org/wiki/SVC#ClearEvent)
         */
        void ClearEvent(DeviceState &state);

        /**
         * @brief Maps the block supplied by the handle (https://switchbrew.org/wiki/SVC#MapSharedMemory)
         */
        void MapSharedMemory(DeviceState &state);

        /**
         * @brief Returns a handle to a KSharedMemory object (https://switchbrew.org/wiki/SVC#CreateTransferMemory)
         */
        void CreateTransferMemory(DeviceState &state);

        /**
         * @brief Closes the specified handle (https://switchbrew.org/wiki/SVC#CloseHandle)
         */
        void CloseHandle(DeviceState &state);

        /**
         * @brief This resets a particular KEvent or KProcess which is signalled (https://switchbrew.org/wiki/SVC#ResetSignal)
         */
        void ResetSignal(DeviceState &state);

        /**
         * @brief Stalls a thread till a KSyncObject signals or the timeout has ended (https://switchbrew.org/wiki/SVC#WaitSynchronization)
         */
        void WaitSynchronization(DeviceState &state);

        /**
         * @brief If the referenced thread is currently in a synchronization call, that call will be interrupted (https://switchbrew.org/wiki/SVC#CancelSynchronization)
         */
        void CancelSynchronization(DeviceState &state);

        /**
         * @brief Locks a specified mutex (https://switchbrew.org/wiki/SVC#ArbitrateLock)
         */
        void ArbitrateLock(DeviceState &state);

        /**
         * @brief Unlocks a specified mutex (https://switchbrew.org/wiki/SVC#ArbitrateUnlock)
         */
        void ArbitrateUnlock(DeviceState &state);

        /**
         * @brief Waits on a process-wide key (Conditional-Variable) (https://switchbrew.org/wiki/SVC#WaitProcessWideKeyAtomic)
         */
        void WaitProcessWideKeyAtomic(DeviceState &state);

        /**
         * @brief Signals a process-wide key (Conditional-Variable) (https://switchbrew.org/wiki/SVC#SignalProcessWideKey)
         */
        void SignalProcessWideKey(DeviceState &state);

        /**
         * @brief This returns the value of CNTPCT_EL0 on the Switch (https://switchbrew.org/wiki/SVC#GetSystemTick)
         */
        void GetSystemTick(DeviceState &state);

        /**
         * @brief Connects to a named IPC port (https://switchbrew.org/wiki/SVC#ConnectToNamedPort)
         */
        void ConnectToNamedPort(DeviceState &state);

        /**
         * @brief Send a synchronous IPC request to a service (https://switchbrew.org/wiki/SVC#SendSyncRequest)
         */
        void SendSyncRequest(DeviceState &state);

        /**
         * @brief Retrieves the PID of a specific thread (https://switchbrew.org/wiki/SVC#GetThreadId)
         */
        void GetThreadId(DeviceState &state);

        /**
         * @brief Outputs a debug string (https://switchbrew.org/wiki/SVC#OutputDebugString)
         */
        void OutputDebugString(DeviceState &state);

        /**
         * @brief Retrieves a piece of information (https://switchbrew.org/wiki/SVC#GetInfo)
         */
        void GetInfo(DeviceState &state);

        /**
         * @brief The SVC Table maps all SVCs to their corresponding functions
         */
        void static (*SvcTable[0x80])(DeviceState &) = {
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
            nullptr, // 0x0E
            nullptr, // 0x0F
            nullptr, // 0x10
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
            nullptr, // 0x2C
            nullptr, // 0x2D
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
}
