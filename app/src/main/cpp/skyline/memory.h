#pragma once

#include "common.h"

namespace skyline::memory {
    /**
     * @brief The Permission struct holds the permission of a particular chunk of memory
     */
    struct Permission {
        /**
         * @brief This constructor initializes all permissions to false
         */
        Permission() {
            r = 0;
            w = 0;
            x = 0;
        };

        /**
         * @param read If memory has read permission
         * @param write If memory has write permission
         * @param execute If memory has execute permission
         */
        Permission(bool read, bool write, bool execute) {
            r = read;
            w = write;
            x = execute;
        };

        /**
         * @brief Equality operator between two Permission objects
         */
        inline bool operator==(const Permission &rhs) const { return (this->r == rhs.r && this->w == rhs.w && this->x == rhs.x); };

        /**
         * @brief Inequality operator between two Permission objects
         */
        inline bool operator!=(const Permission &rhs) const { return !operator==(rhs); };

        /**
         * @return The value of the permission struct in mmap(2) format
         */
        int Get() const {
            int perm = 0;
            if (r)
                perm |= PROT_READ;
            if (w)
                perm |= PROT_WRITE;
            if (x)
                perm |= PROT_EXEC;
            return perm;
        };

        bool r;
        bool w;
        bool x;
    };

    /**
     * @brief This holds certain attributes of a chunk of memory: https://switchbrew.org/wiki/SVC#MemoryAttribute
     */
    union MemoryAttribute {
        struct {
            bool isBorrowed     : 1;
            bool isIpcLocked    : 1;
            bool isDeviceShared : 1;
            bool isUncached     : 1;
        };
        u32 value;
    };
    static_assert(sizeof(MemoryAttribute) == sizeof(u32));

    /**
     * @brief This describes the properties of a region of the allocated memory
     */
    struct RegionInfo {
        u64 address; //!< The starting address of the chunk of memory
        u64 size; //!< The size of the chunk of memory
        bool isUncached; //!< If the following region is uncached

        RegionInfo(u64 address, u64 size, bool isUncached) : address(address), size(size), isUncached(isUncached) {}
    };

    /**
     * @brief This contains information about a chunk of memory: https://switchbrew.org/wiki/SVC#MemoryInfo
     */
    struct MemoryInfo {
        u64 baseAddress;
        u64 size;
        u32 type;
        MemoryAttribute memoryAttribute;
        union {
            u32 _pad0_;
            struct {
                bool r : 1, w : 1, x : 1;
            };
        };
        u32 ipcRefCount;
        u32 deviceRefCount;
        u32                          : 32;
    };
    static_assert(sizeof(MemoryInfo) == 0x28);

    /**
     * @brief These are specific markers for the type of a memory region
     */
    enum class Type : u32 {
        Unmapped = 0x00000000,
        Io = 0x00002001,
        Normal = 0x00042002,
        CodeStatic = 0x00DC7E03,
        CodeMutable = 0x03FEBD04,
        Heap = 0x037EBD05,
        SharedMemory = 0x00402006,
        Alias = 0x00482907,
        ModuleCodeStatic = 0x00DD7E08,
        ModuleCodeMutable = 0x03FFBD09,
        Ipc = 0x005C3C0A,
        Stack = 0x005C3C0B,
        ThreadLocal = 0x0040200C,
        TransferMemoryIsolated = 0x015C3C0D,
        TransferMemory = 0x005C380E,
        ProcessMemory = 0x0040380F,
        Reserved = 0x00000010,
        NonSecureIpc = 0x005C3811,
        NonDeviceIpc = 0x004C2812,
        KernelStack = 0x00002013,
        CodeReadOnly = 0x00402214,
        CodeWritable = 0x00402015
    };

    /**
     * @brief Memory Regions that are mapped by the kernel
     */
    enum class Region {
        Heap, Text, RoData, Data, Bss
    };
}
