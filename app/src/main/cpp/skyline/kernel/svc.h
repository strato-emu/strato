#pragma once

#include "ipc.h"
#include <common.h>

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
    };
    namespace kernel::svc {
        /**
         * @brief Set the process heap to a given size (https://switchbrew.org/wiki/SVC#svcSetHeapSize)
         */
        void SetHeapSize(DeviceState &state);

        /**
         * @brief Change attribute of page-aligned memory region. This is used to turn on/off caching for a given memory area. (https://switchbrew.org/wiki/SVC#svcSetMemoryAttribute)
         */
        void SetMemoryAttribute(DeviceState &state);

        /**
         * @brief Query information about an address (https://switchbrew.org/wiki/SVC#svcQueryMemory)
         */
        void QueryMemory(DeviceState &state);

        /**
         * @brief Exits the current process (https://switchbrew.org/wiki/SVC#svcExitProcess)
         */
        void ExitProcess(DeviceState &state);

        /**
         * @brief Create a thread in the current process (https://switchbrew.org/wiki/SVC#svcCreateThread)
         */
        void CreateThread(DeviceState &state);

        /**
         * @brief Starts the thread for the provided handle (https://switchbrew.org/wiki/SVC#svcStartThread)
         */
        void StartThread(DeviceState &state);

        /**
         * @brief Exits the current thread (https://switchbrew.org/wiki/SVC#svcExitThread)
         */
        void ExitThread(DeviceState &state);

        /**
         * @brief Sleep for a specified amount of time, or yield thread (https://switchbrew.org/wiki/SVC#svcExitThread)
         */
        void SleepThread(DeviceState &state);

        /**
         * @brief Get priority of provided thread handle (https://switchbrew.org/wiki/SVC#svcGetThreadPriority)
         */
        void GetThreadPriority(DeviceState &state);

        /**
         * @brief Set priority of provided thread handle (https://switchbrew.org/wiki/SVC#svcSetThreadPriority)
         */
        void SetThreadPriority(DeviceState &state);

        /**
         * @brief Maps the block supplied by the handle (https://switchbrew.org/wiki/SVC#svcMapSharedMemory)
         */
        void MapSharedMemory(DeviceState &state);

        /**
         * @brief Closes the specified handle
         */
        void CloseHandle(DeviceState &state);

        /**
         * @brief Returns a handle to a KSharedMemory object (https://switchbrew.org/wiki/SVC#svcCreateTransferMemory)
         */
        void CreateTransferMemory(DeviceState &state);

        /**
         * @brief This resets a particular KEvent or KProcess which is signalled (https://switchbrew.org/wiki/SVC#svcResetSignal)
         */
        void ResetSignal(DeviceState &state);

        /**
         * @brief Stalls a thread till a KSyncObject signals or the timeout has ended (https://switchbrew.org/wiki/SVC#svcWaitSynchronization)
         */
        void WaitSynchronization(DeviceState &state);

        /**
         * @brief This returns the value of CNTPCT_EL0 on the Switch (https://switchbrew.org/wiki/SVC#svcGetSystemTick)
         */
        void GetSystemTick(DeviceState &state);

        /**
         * @brief Locks a specified mutex
         */
        void ArbitrateLock(DeviceState &state);

        /**
         * @brief Unlocks a specified mutex
         */
        void ArbitrateUnlock(DeviceState &state);

        /**
         * @brief Waits on a process-wide key (Conditional-Variable)
         */
        void WaitProcessWideKeyAtomic(DeviceState &state);

        /**
         * @brief Signals a process-wide key (Conditional-Variable)
         */
        void SignalProcessWideKey(DeviceState &state);

        /**
         * @brief Connects to a named IPC port
         */
        void ConnectToNamedPort(DeviceState &state);

        /**
         * @brief Send a synchronous IPC request to a service
         */
        void SendSyncRequest(DeviceState &state);

        /**
         * @brief Retrieves the PID of a specific thread
         */
        void GetThreadId(DeviceState &state);

        /**
         * @brief Outputs a debug string
         */
        void OutputDebugString(DeviceState &state);

        /**
         * @brief Retrieves a piece of information (https://switchbrew.org/wiki/SVC#svcGetInfo)
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
            nullptr, // 0x04
            nullptr, // 0x05
            QueryMemory, // 0x06
            ExitProcess, // 0x07
            CreateThread, // 0x08
            StartThread, // 0x09
            ExitThread, // 0x0a
            SleepThread, // 0x0b
            GetThreadPriority, // 0x0c
            SetThreadPriority, // 0x0d
            nullptr, // 0x0e
            nullptr, // 0x0f
            nullptr, // 0x10
            nullptr, // 0x11
            nullptr, // 0x12
            MapSharedMemory, // 0x13
            nullptr, // 0x14
            CreateTransferMemory, // 0x15
            CloseHandle, // 0x16
            ResetSignal, // 0x17
            WaitSynchronization, // 0x18
            nullptr, // 0x19
            ArbitrateLock, // 0x1a
            ArbitrateUnlock, // 0x1b
            WaitProcessWideKeyAtomic, // 0x1c
            SignalProcessWideKey, // 0x1d
            GetSystemTick, // 0x1e
            ConnectToNamedPort, // 0x1f
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
            nullptr, // 0x2a
            nullptr, // 0x2b
            nullptr, // 0x2c
            nullptr, // 0x2d
            nullptr, // 0x2e
            nullptr, // 0x2f
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
            nullptr, // 0x3a
            nullptr, // 0x3b
            nullptr, // 0x3c
            nullptr, // 0x3d
            nullptr, // 0x3e
            nullptr, // 0x3f
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
            nullptr, // 0x4a
            nullptr, // 0x4b
            nullptr, // 0x4c
            nullptr, // 0x4d
            nullptr, // 0x4e
            nullptr, // 0x4f
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
            nullptr, // 0x5a
            nullptr, // 0x5b
            nullptr, // 0x5c
            nullptr, // 0x5d
            nullptr, // 0x5e
            nullptr, // 0x5f
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
            nullptr, // 0x6a
            nullptr, // 0x6b
            nullptr, // 0x6c
            nullptr, // 0x6d
            nullptr, // 0x6e
            nullptr, // 0x6f
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
            nullptr, // 0x7a
            nullptr, // 0x7b
            nullptr, // 0x7c
            nullptr, // 0x7d
            nullptr, // 0x7e
            nullptr // 0x7f
        };
    }
}
