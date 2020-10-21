// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline {
    namespace memory {
        struct Permission {
            /**
             * @brief Initializes all permissions to false
             */
            constexpr Permission() : r(), w(), x() {}

            /**
             * @param read If memory has read permission
             * @param write If memory has write permission
             * @param execute If memory has execute permission
             */
            constexpr Permission(bool read, bool write, bool execute) : r(read), w(write), x(execute) {}

            inline bool operator==(const Permission &rhs) const { return (this->r == rhs.r && this->w == rhs.w && this->x == rhs.x); }

            inline bool operator!=(const Permission &rhs) const { return !operator==(rhs); }

            /**
             * @return The value of the permission struct in Linux format
             */
            constexpr int Get() const {
                int perm{};
                if (r)
                    perm |= PROT_READ;
                if (w)
                    perm |= PROT_WRITE;
                if (x)
                    perm |= PROT_EXEC;
                return perm;
            }

            bool r; //!< The permission to read
            bool w; //!< The permission to write
            bool x; //!< The permission to execute
        };

        /**
         * @url https://switchbrew.org/wiki/SVC#MemoryAttribute
         */
        union MemoryAttribute {
            struct {
                bool isBorrowed     : 1; //!< This is required for async IPC user buffers
                bool isIpcLocked    : 1; //!< True when IpcRefCount > 0
                bool isDeviceShared : 1; //!< True when DeviceRefCount > 0
                bool isUncached     : 1; //!< This is used to disable memory caching to share memory with the GPU
            };
            u32 value{};
        };

        /**
         * @url https://switchbrew.org/wiki/SVC#MemoryInfo
         */
        struct MemoryInfo {
            u64 address; //!< The base address of the mapping
            u64 size; //!< The size of the mapping
            u32 type; //!< The MemoryType of the mapping
            u32 attributes; //!< The attributes of the mapping
            u32 permissions; //!< The permissions of the mapping
            u32 ipcRefCount; //!< The IPC reference count (Always 0)
            u32 deviceRefCount; //!< The device reference count (Always 0)
            u32 _pad0_;
        };
        static_assert(sizeof(MemoryInfo) == 0x28);

        /**
         * @brief These are specific markers for the type of a memory region
         * @url https://switchbrew.org/wiki/SVC#MemoryType
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
            CodeWritable = 0x15,
        };

        /**
         * @url https://switchbrew.org/wiki/SVC#MemoryState
         */
        union MemoryState {
            constexpr MemoryState(const u32 value) : value(value) {}

            constexpr MemoryState() : value(0) {}

            constexpr bool operator==(const MemoryState& other) const {
                return value == other.value;
            }

            constexpr bool operator!=(const MemoryState& other) const {
                return value != other.value;
            }

            struct {
                MemoryType type;
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
            u32 value{};
        };
        static_assert(sizeof(MemoryState) == sizeof(u32));

        /**
         * @brief The preset states that different regions are set to
         * @url https://switchbrew.org/wiki/SVC#MemoryType
         */
        namespace states {
            constexpr MemoryState Unmapped{0x00000000};
            constexpr MemoryState Io{0x00002001};
            constexpr MemoryState CodeStatic{0x00DC7E03};
            constexpr MemoryState CodeMutable{0x03FEBD04};
            constexpr MemoryState Heap{0x037EBD05};
            constexpr MemoryState SharedMemory{0x00402006};
            constexpr MemoryState Alias{0x00482907};
            constexpr MemoryState AliasCode{0x00DD7E08};
            constexpr MemoryState AliasCodeData{0x03FFBD09};
            constexpr MemoryState Ipc{0x005C3C0A};
            constexpr MemoryState Stack{0x005C3C0B};
            constexpr MemoryState ThreadLocal{0x0040200C};
            constexpr MemoryState TransferMemoryIsolated{0x015C3C0D};
            constexpr MemoryState TransferMemory{0x005C380E};
            constexpr MemoryState SharedCode{0x0040380F};
            constexpr MemoryState Reserved{0x00000010};
            constexpr MemoryState NonSecureIpc{0x005C3811};
            constexpr MemoryState NonDeviceIpc{0x004C2812};
            constexpr MemoryState KernelStack{0x00002013};
            constexpr MemoryState CodeReadOnly{0x00402214};
            constexpr MemoryState CodeWritable{0x00402015};
        };

        struct Region {
            u64 address;
            size_t size;

            bool IsInside(void* ptr) {
                return (address <= reinterpret_cast<u64>(ptr)) && ((address + size) > reinterpret_cast<u64>(ptr));
            }
        };

        enum class AddressSpaceType : u8 {
            AddressSpace32Bit = 0, //!< 32-bit address space used by 32-bit applications
            AddressSpace36Bit = 1, //!< 36-bit address space used by 64-bit applications before 2.0.0
            AddressSpace32BitNoReserved = 2, //!< 32-bit address space without the map region
            AddressSpace39Bit, //!< 39-bit address space used by 64-bit applications after 2.0.0
        };
    }

    namespace loader {
        class NroLoader;
        class NsoLoader;
        class NcaLoader;
    }

    namespace kernel {
        struct ChunkDescriptor {
            u8* ptr;
            size_t size;
            memory::Permission permission;
            memory::MemoryState state;
            memory::MemoryAttribute attributes;

            constexpr bool IsCompatible(const ChunkDescriptor& chunk) const {
                return chunk.permission == permission && chunk.state.value == state.value && chunk.attributes.value == attributes.value;
            }
        };

        /**
         * @brief MemoryManager keeps track of guest virtual memory and it's related attributes
         */
        class MemoryManager {
          private:
            const DeviceState &state;
            std::vector<ChunkDescriptor> chunks;

          public:
            memory::Region addressSpace{}; //!< The entire address space
            memory::Region base{}; //!< The application-accessible address space
            memory::Region code{};
            memory::Region alias{};
            memory::Region heap{};
            memory::Region stack{};
            memory::Region tlsIo{}; //!< TLS/IO

            std::shared_mutex mutex; //!< Synchronizes any operations done on the VMM, it is locked in shared mode by readers and exclusive mode by writers

            MemoryManager(const DeviceState &state);

            /**
             * @note This should be called before any mappings in the VMM or calls to InitalizeRegions are done
             */
            void InitializeVmm(memory::AddressSpaceType type);

            void InitializeRegions(u8* codeStart, u64 size);

            void InsertChunk(const ChunkDescriptor &chunk);

            std::optional<ChunkDescriptor> Get(void* ptr);

            /**
             * @return The cumulative size of all memory mappings in bytes
             */
            size_t GetMemoryUsage();

            /**
             * @return The total page-aligned size used to store memory block metadata, if they were KMemoryBlocks rather than ChunkDescriptor
             */
            size_t GetKMemoryBlockSize();
        };
    }
}
