// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <forward_list>
#include <common.h>
#include "types/KObject.h"

namespace skyline {
    namespace memory {
        /**
         * @brief The Permission struct holds the permission of a particular chunk of memory
         */
        struct Permission {
            /**
             * @brief This constructor initializes all permissions to false
             */
            Permission() : r(), w(), x() {};

            /**
             * @param read If memory has read permission
             * @param write If memory has write permission
             * @param execute If memory has execute permission
             */
            Permission(bool read, bool write, bool execute) : r(read), w(write), x(execute) {};

            /**
             * @brief Equality operator between two Permission objects
             */
            inline bool operator==(const Permission &rhs) const { return (this->r == rhs.r && this->w == rhs.w && this->x == rhs.x); };

            /**
             * @brief Inequality operator between two Permission objects
             */
            inline bool operator!=(const Permission &rhs) const { return !operator==(rhs); };

            /**
             * @return The value of the permission struct in Linux format
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

            bool r; //!< The permission to read
            bool w; //!< The permission to write
            bool x; //!< The permission to execute
        };

        /**
         * @brief This holds certain attributes of a chunk of memory: https://switchbrew.org/wiki/SVC#MemoryAttribute
         */
        union MemoryAttribute {
            struct {
                bool isBorrowed     : 1; //!< This is required for async IPC user buffers
                bool isIpcLocked    : 1; //!< True when IpcRefCount > 0
                bool isDeviceShared : 1; //!< True when DeviceRefCount > 0
                bool isUncached     : 1; //!< This is used to disable memory caching to share memory with the GPU
            };
            u32 value;
        };

        /**
         * @brief This contains information about a chunk of memory: https://switchbrew.org/wiki/SVC#MemoryInfo
         */
        struct MemoryInfo {
            u64 address; //!< The base address of the mapping
            u64 size; //!< The size of the mapping
            u32 type; //!< The MemoryType of the mapping
            u32 attributes; //!< The attributes of the mapping
            u32 permissions; //!< The permissions of the mapping
            u32 ipcRefCount; //!< The IPC reference count (This is always 0)
            u32 deviceRefCount; //!< The device reference count (This is always 0)
            u32 _pad0_;
        };
        static_assert(sizeof(MemoryInfo) == 0x28);

        /**
         * @brief These are specific markers for the type of a memory region (https://switchbrew.org/wiki/SVC#MemoryType)
         */
        enum class MemoryType : u8 {
            Unmapped = 0x0,
            Io = 0x1,
            Normal = 0x2,
            CodeStatic = 0x3,
            CodeMutable = 0x4,
            Heap = 0x5,
            SharedMemory = 0x6,
            Alias = 0x7,
            ModuleCodeStatic = 0x8,
            ModuleCodeMutable = 0x9,
            Ipc = 0xA,
            Stack = 0xB,
            ThreadLocal = 0xC,
            TransferMemoryIsolated = 0xD,
            TransferMemory = 0xE,
            ProcessMemory = 0xF,
            Reserved = 0x10,
            NonSecureIpc = 0x11,
            NonDeviceIpc = 0x12,
            KernelStack = 0x13,
            CodeReadOnly = 0x14,
            CodeWritable = 0x15
        };

        /**
         * @brief This structure is used to hold the state of a certain block of memory (https://switchbrew.org/wiki/SVC#MemoryState)
         */
        union MemoryState {
            constexpr MemoryState(const u32 value) : value(value) {};

            constexpr MemoryState() : value(0) {};

            struct {
                MemoryType type; //!< The MemoryType of this memory block
                bool permissionChangeAllowed : 1; //!< If the application can use svcSetMemoryPermission on this block
                bool forceReadWritableByDebugSyscalls : 1; //!< If the application can use svcWriteDebugProcessMemory on this block
                bool ipcSendAllowed : 1; //!< If this block is allowed to be sent as an IPC buffer with flags=0
                bool nonDeviceIpcSendAllowed : 1; //!< If this block is allowed to be sent as an IPC buffer with flags=3
                bool nonSecureIpcSendAllowed : 1; //!< If this block is allowed to be sent as an IPC buffer with flags=1
                bool _pad0_ : 1;
                bool processPermissionChangeAllowed : 1; //!< If the application can use svcSetProcessMemoryPermission on this block
                bool mapAllowed : 1; //!< If the application can use svcMapMemory on this block
                bool unmapProcessCodeMemoryAllowed : 1; //!< If the application can use svcUnmapProcessCodeMemory on this block
                bool transferMemoryAllowed : 1; //!< If the application can use svcCreateTransferMemory on this block
                bool queryPhysicalAddressAllowed : 1; //!< If the application can use svcQueryPhysicalAddress on this block
                bool mapDeviceAllowed : 1; //!< If the application can use svcMapDeviceAddressSpace or svcMapDeviceAddressSpaceByForce on this block
                bool mapDeviceAlignedAllowed : 1; //!< If the application can use svcMapDeviceAddressSpaceAligned on this block
                bool ipcBufferAllowed : 1; //!< If the application can use this block with svcSendSyncRequestWithUserBuffer
                bool isReferenceCounted : 1; //!< If the physical memory blocks backing this region are reference counted
                bool mapProcessAllowed : 1; //!< If the application can use svcMapProcessMemory on this block
                bool attributeChangeAllowed : 1; //!< If the application can use svcSetMemoryAttribute on this block
                bool codeMemoryAllowed : 1; //!< If the application can use svcCreateCodeMemory on this block
            };
            u32 value;
        };
        static_assert(sizeof(MemoryState) == sizeof(u32));

        /**
         * @brief The preset states that different regions are set to (https://switchbrew.org/wiki/SVC#MemoryType)
         */
        namespace states {
            constexpr MemoryState Unmapped = 0x00000000;
            constexpr MemoryState Io = 0x00002001;
            constexpr MemoryState CodeStatic = 0x00DC7E03;
            constexpr MemoryState CodeMutable = 0x03FEBD04;
            constexpr MemoryState Heap = 0x037EBD05;
            constexpr MemoryState SharedMemory = 0x00402006;
            constexpr MemoryState Alias = 0x00482907;
            constexpr MemoryState AliasCode = 0x00DD7E08;
            constexpr MemoryState AliasCodeData = 0x03FFBD09;
            constexpr MemoryState Ipc = 0x005C3C0A;
            constexpr MemoryState Stack = 0x005C3C0B;
            constexpr MemoryState ThreadLocal = 0x0040200C;
            constexpr MemoryState TransferMemoryIsolated = 0x015C3C0D;
            constexpr MemoryState TransferMemory = 0x005C380E;
            constexpr MemoryState SharedCode = 0x0040380F;
            constexpr MemoryState Reserved = 0x00000010;
            constexpr MemoryState NonSecureIpc = 0x005C3811;
            constexpr MemoryState NonDeviceIpc = 0x004C2812;
            constexpr MemoryState KernelStack = 0x00002013;
            constexpr MemoryState CodeReadOnly = 0x00402214;
            constexpr MemoryState CodeWritable = 0x00402015;
        };

        /**
         * @brief This enumerates all of the memory regions in the process address space
         */
        enum class Regions {
            Base, //!< The region representing the entire address space
            Code, //!< The code region contains all of the loaded in code
            Alias, //!< The alias region is reserved for allocating thread stack before 2.0.0
            Heap, //!< The heap region is reserved for heap allocations
            Stack, //!< The stack region is reserved for allocating thread stack after 2.0.0
            TlsIo, //!< The TLS/IO region is reserved for allocating TLS and Device MMIO
        };

        /**
         * @brief This struct is used to hold the location and size of a memory region
         */
        struct Region {
            Regions id; //!< The ID of the region
            u64 address; //!< The base address of the region
            u64 size; //!< The size of the region in bytes

            /**
             * @brief Checks if the specified address is within the region
             * @param address The address to check
             * @return If the address is inside the region
             */
            inline bool IsInside(u64 address) {
                return (this->address <= address) && ((this->address + this->size) > address);
            }
        };

        /**
         * @brief The type of the address space used by an application
         */
        enum class AddressSpaceType {
            AddressSpace32Bit, //!< 32-bit address space used by 32-bit applications
            AddressSpace36Bit, //!< 36-bit address space used by 64-bit applications before 2.0.0
            AddressSpace39Bit, //!< 39-bit address space used by 64-bit applications after 2.0.0
        };
    }

    namespace loader {
        class NroLoader;
    }

    namespace kernel {
        namespace type {
            class KPrivateMemory;
            class KSharedMemory;
            class KTransferMemory;
        }

        namespace svc {
            void SetMemoryAttribute(DeviceState &state);

            void MapMemory(DeviceState &state);
        }

        /**
         * @brief This describes a single block of memory and all of it's individual attributes
         */
        struct BlockDescriptor {
            u64 address; //!< The address of the current block
            u64 size; //!< The size of the current block in bytes
            memory::Permission permission; //!< The permissions applied to the current block
            memory::MemoryAttribute attributes; //!< The MemoryAttribute for the current block
        };

        /**
         * @brief This describes a single chunk of memory, this is owned by a memory backing
         */
        struct ChunkDescriptor {
            u64 address; //!< The address of the current chunk
            u64 size; //!< The size of the current chunk in bytes
            u64 host; //!< The address of the chunk in the host
            memory::MemoryState state; //!< The MemoryState for the current block
            std::vector<BlockDescriptor> blockList; //!< This vector holds the block descriptors for all the children blocks of this Chunk
        };

        /**
         * @brief This contains both of the descriptors for a specific address
         */
        struct DescriptorPack {
            const BlockDescriptor block; //!< The block descriptor at the address
            const ChunkDescriptor chunk; //!< The chunk descriptor at the address
        };

        /**
         * @brief The MemoryManager class handles the memory map and the memory regions of the process
         */
        class MemoryManager {
          private:
            const DeviceState &state; //!< The state of the device
            std::vector<ChunkDescriptor> chunkList; //!< This vector holds all the chunk descriptors
            memory::Region base{memory::Regions::Base}; //!< The Region object for the entire address space
            memory::Region code{memory::Regions::Code}; //!< The Region object for the code memory region
            memory::Region alias{memory::Regions::Alias}; //!< The Region object for the alias memory region
            memory::Region heap{memory::Regions::Heap}; //!< The Region object for the heap memory region
            memory::Region stack{memory::Regions::Stack}; //!< The Region object for the stack memory region
            memory::Region tlsIo{memory::Regions::TlsIo}; //!< The Region object for the TLS/IO memory region

            /**
             * @param address The address to find a chunk at
             * @return A pointer to the ChunkDescriptor or nullptr in case chunk was not found
             */
            ChunkDescriptor *GetChunk(u64 address);

            /**
             * @param address The address to find a block at
             * @return A pointer to the BlockDescriptor or nullptr in case chunk was not found
             */
            BlockDescriptor *GetBlock(u64 address, ChunkDescriptor* chunk = nullptr);

            /**
             * @brief Inserts a chunk into the memory map
             * @param chunk The chunk to insert
             */
            void InsertChunk(const ChunkDescriptor &chunk);

            /**
             * @brief Deletes a chunk located at the address from the memory map
             * @param address The address of the chunk to delete
             */
            void DeleteChunk(u64 address);

            /**
             * @brief Resize the specified chunk to the specified size
             * @param chunk The chunk to resize
             * @param size The new size of the chunk
             */
            static void ResizeChunk(ChunkDescriptor *chunk, size_t size);

            /**
             * @brief Insert a block into a chunk
             * @param chunk The chunk to insert the block into
             * @param block The block to insert
             */
            static void InsertBlock(ChunkDescriptor *chunk, const BlockDescriptor block);

            /**
             * @brief This initializes all of the regions in the address space
             * @param address The starting address of the code region
             * @param size The size of the code region
             * @param type The type of the address space
             */
            void InitializeRegions(u64 address, u64 size, const memory::AddressSpaceType type);

          public:
            friend class type::KPrivateMemory;
            friend class type::KSharedMemory;
            friend class type::KTransferMemory;
            friend class type::KProcess;
            friend class loader::NroLoader;

            friend void svc::SetMemoryAttribute(DeviceState &state);

            friend void svc::MapMemory(skyline::DeviceState &state);

            MemoryManager(const DeviceState &state);

            /**
             * @param address The address to query in the memory map
             * @return A DescriptorPack retrieved from the memory map
             */
            std::optional<DescriptorPack> Get(u64 address);

            /**
             * @param region The region to retrieve
             * @return A Region object for the specified region
             */
            memory::Region GetRegion(memory::Regions region);

            /**
             * @brief The total amount of space in bytes occupied by all memory mappings
             * @return The cumulative size of all memory mappings in bytes
             */
            size_t GetProgramSize();
        };
    }
}
