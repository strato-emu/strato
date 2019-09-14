#pragma once

#include "common.h"

namespace lightSwitch::Memory {
    /**
     * The Permission struct holds the permission of a particular chunk of memory
     */
    struct Permission {
        /**
         * Initializes all values to false
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
         * Equality operator between two Permission objects
         */
        bool operator==(const Permission &rhs) const { return (this->r == rhs.r && this->w == rhs.w && this->x == rhs.x); };

        /**
         * Inequality operator between two Permission objects
         */
        bool operator!=(const Permission &rhs) const { return !operator==(rhs); };

        /**
         * @return The value of the permission struct in mmap(2) format
         */
        int get() const {
            int perm = 0;
            if (r) perm |= PROT_READ;
            if (w) perm |= PROT_WRITE;
            if (x) perm |= PROT_EXEC;
            return perm;
        };

        bool r : 1, w : 1, x : 1;
    };

    /**
     * This holds certain attributes of a chunk of memory: https://switchbrew.org/wiki/SVC#MemoryAttribute
     */
    struct MemoryAttribute {
        bool IsBorrowed : 1;
        bool IsIpcLocked : 1;
        bool IsDeviceShared : 1;
        bool IsUncached : 1;
    };

    /**
     * This contains information about a chunk of memory: https://switchbrew.org/wiki/SVC#MemoryInfo
     */
    struct MemoryInfo {
        u64 base_address : 64;
        u64 size : 64;
        u64 type : 64;
        MemoryAttribute memory_attribute;
        Permission perms;
        u32 ipc_ref_count : 32;
        u32 device_ref_count : 32;
        u32 : 32;
    };
    static_assert(sizeof(MemoryInfo) == 0x28);

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
     * Memory Regions that are mapped by the kernel
     */
    enum class Region {
        heap, text, rodata, data, bss
    };
}
