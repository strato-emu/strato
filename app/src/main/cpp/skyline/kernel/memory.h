// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <sys/mman.h>
#include <common.h>
#include <common/file_descriptor.h>

namespace skyline {
    namespace kernel::type {
        class KMemory;
    }

    namespace memory {
        union Permission {
            /**
             * @brief Initializes all permissions to false
             */
            constexpr Permission() : r(), w(), x() {}

            /**
             * @brief Initializes permissions where the first three bits correspond to RWX
             */
            constexpr explicit Permission(u8 raw) : raw(raw) {}

            /**
             * @param read If memory has read permission
             * @param write If memory has write permission
             * @param execute If memory has execute permission
             */
            constexpr Permission(bool read, bool write, bool execute) : r(read), w(write), x(execute) {}

            inline bool operator==(const Permission &rhs) const { return r == rhs.r && w == rhs.w && x == rhs.x; }

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

            struct {
                bool r : 1; //!< The permission to read
                bool w : 1; //!< The permission to write
                bool x : 1; //!< The permission to execute
            };
            u8 raw;
        };
        static_assert(sizeof(Permission) == sizeof(u8));

        /**
         * @url https://switchbrew.org/wiki/SVC#MemoryAttribute
         */
        union MemoryAttribute {
            struct {
                bool isBorrowed : 1; //!< This is required for async IPC user buffers
                bool isIpcLocked : 1; //!< True when IpcRefCount > 0
                bool isDeviceShared : 1; //!< True when DeviceRefCount > 0
                bool isUncached : 1; //!< This is used to disable memory caching to share memory with the GPU
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

            constexpr bool operator==(const MemoryState &other) const {
                return value == other.value;
            }

            constexpr bool operator!=(const MemoryState &other) const {
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
        }

        enum class AddressSpaceType : u8 {
            AddressSpace32Bit = 0, //!< 32-bit address space used by 32-bit applications
            AddressSpace36Bit = 1, //!< 36-bit address space used by 64-bit applications before 2.0.0
            AddressSpace32BitNoReserved = 2, //!< 32-bit address space without the map region
            AddressSpace39Bit = 3, //!< 39-bit address space used by 64-bit applications after 2.0.0
        };
    }

    namespace kernel {
        struct ChunkDescriptor {
            u8 *ptr;
            size_t size;
            memory::Permission permission;
            memory::MemoryState state;
            memory::MemoryAttribute attributes;
            kernel::type::KMemory *memory{};

            constexpr bool IsCompatible(const ChunkDescriptor &chunk) const {
                return chunk.permission == permission && chunk.state.value == state.value && chunk.attributes.value == attributes.value && chunk.memory == memory;
            }
        };

        /**
         * @brief MemoryManager allocates and keeps track of guest virtual memory and its related attributes
         */
        class MemoryManager {
          private:
            const DeviceState &state;
            std::vector<ChunkDescriptor> chunks;

          public:
            memory::AddressSpaceType addressSpaceType{};
            span<u8> addressSpace{}; //!< The entire address space
            span<u8> codeBase36Bit{}; //!< A mapping in the lower 36 bits of the address space for mapping code and stack on 36-bit guests
            span<u8> base{}; //!< The application-accessible address space (for 39-bit guests) or the heap/alias address space (for 36-bit guests)
            span<u8> code{};
            span<u8> alias{};
            span<u8> heap{};
            span<u8> stack{};
            span<u8> tlsIo{}; //!< TLS/IO

            std::shared_mutex mutex; //!< Synchronizes any operations done on the VMM, it's locked in shared mode by readers and exclusive mode by writers

            MemoryManager(const DeviceState &state);

            ~MemoryManager();

            /**
             * @note This should be called before any mappings in the VMM or calls to InitalizeRegions are done
             */
            void InitializeVmm(memory::AddressSpaceType type);

            void InitializeRegions(span<u8> codeRegion);

            /**
             * @brief Mirrors a page-aligned mapping in the guest address space to the host address space
             * @return A span to the host address space mirror mapped as RWX, unmapping it is the responsibility of the caller
             * @note The supplied mapping **must** be page-aligned and inside the guest address space
             */
            span<u8> CreateMirror(span<u8> mapping);

            /**
             * @brief Mirrors multiple page-aligned mapping in the guest address space to the host address space
             * @param totalSize The total size of all the regions to be mirrored combined
             * @return A span to the host address space mirror mapped as RWX, unmapping it is the responsibility of the caller
             * @note The supplied mapping **must** be page-aligned and inside the guest address space
             * @note If a single mapping is mirrored, it is recommended to use CreateMirror instead
             */
            span<u8> CreateMirrors(const std::vector<span<u8>> &regions);

            /**
             * @brief Frees the underlying physical memory for all full pages in the contained mapping
             * @note All subsequent accesses to freed memory will return 0s
             */
            void FreeMemory(span<u8> memory);

            void InsertChunk(const ChunkDescriptor &chunk);

            std::optional<ChunkDescriptor> Get(void *ptr);

            /**
             * @return The cumulative size of all heap (Physical Memory + Process Heap) memory mappings, the code region and the main thread stack in bytes
             */
            size_t GetUserMemoryUsage();

            /**
             * @return The total page-aligned size used to store memory block metadata, if they were KMemoryBlocks rather than ChunkDescriptor
             * @note There is a ceiling of SystemResourceSize as specified in the NPDM, this value will be clipped to that
             */
            size_t GetSystemResourceUsage();

            /**
             * @return If the supplied region is contained withing the accessible guest address space
             */
            bool AddressSpaceContains(span<u8> region) const {
                if (addressSpaceType == memory::AddressSpaceType::AddressSpace36Bit)
                    return codeBase36Bit.contains(region) || base.contains(region);
                else
                    return base.contains(region);
            }
        };
    }
}
