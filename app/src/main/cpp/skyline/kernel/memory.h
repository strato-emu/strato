// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <sys/mman.h>
#include <common.h>
#include <common/file_descriptor.h>
#include <map>

namespace skyline {
    namespace kernel::type {
        class KMemory;
    }

    namespace memory {
        union Permission {
            /**
             * @brief Initializes all permissions to false
             */
            constexpr Permission() : raw() {}

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
            Static = 0x2,
            Code = 0x3,
            CodeMutable = 0x4,
            Heap = 0x5,
            SharedMemory = 0x6,

            AliasCode = 0x8,
            AliasCodeData = 0x9,
            Ipc = 0xA,
            Stack = 0xB,
            ThreadLocal = 0xC,
            TransferMemoryIsolated = 0xD,
            TransferMemory = 0xE,
            SharedCode = 0xF,
            Reserved = 0x10,
            NonSecureIpc = 0x11,
            NonDeviceIpc = 0x12,
            KernelStack = 0x13,
            CodeGenerated = 0x14,
            CodeExternal = 0x15,
            Coverage = 0x16,
            InsecureMemory = 0x17
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
                bool isMappedInKernel : 1; //!< If this block is mapped in kernel
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
                bool isLinearMapped : 1; //!< If this block is mapped linearly
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
            constexpr MemoryState Io{0x00182001};
            constexpr MemoryState Static{0x00042002};
            constexpr MemoryState Code{0x04DC7E03};
            constexpr MemoryState CodeMutable{0x07FEBD04};
            constexpr MemoryState Heap{0x077EBD05};
            constexpr MemoryState SharedMemory{0x04402006};

            constexpr MemoryState AliasCode{0x04DD7E08};
            constexpr MemoryState AliasCodeData{0x07FFBD09};
            constexpr MemoryState Ipc{0x045C3C0A};
            constexpr MemoryState Stack{0x045C3C0B};
            constexpr MemoryState ThreadLocal{0x0400200C};
            constexpr MemoryState TransferMemoryIsolated{0x055C3C0D};
            constexpr MemoryState TransferMemory{0x045C380E};
            constexpr MemoryState SharedCode{0x0440380F};
            constexpr MemoryState Reserved{0x00000010};
            constexpr MemoryState NonSecureIpc{0x045C3811};
            constexpr MemoryState NonDeviceIpc{0x044C2812};
            constexpr MemoryState KernelStack{0x00002013};
            constexpr MemoryState CodeGenerated{0x04402214};
            constexpr MemoryState CodeExternal{0x04402015};
            constexpr MemoryState Coverage{0x00002016};
            constexpr MemoryState InsecureMemory{0x05583817};
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
            bool isSrcMergeDisallowed;
            size_t size;
            memory::Permission permission;
            memory::MemoryState state;
            memory::MemoryAttribute attributes;

            constexpr bool IsCompatible(const ChunkDescriptor &chunk) const noexcept {
                return chunk.permission == permission && chunk.state.value == state.value && chunk.attributes.value == attributes.value && !isSrcMergeDisallowed;
            }
        };

        /**
         * @brief MemoryManager allocates and keeps track of guest virtual memory and its related attributes
         */
        class MemoryManager {
          private:
            const DeviceState &state;
            std::map<u8 *, ChunkDescriptor> chunks;

            std::vector<std::shared_ptr<type::KMemory>> memRefs;

            // Workaround for broken std implementation
            std::map<u8 *, ChunkDescriptor>::iterator upper_bound(u8 *address);

            void MapInternal(std::pair<u8 *, ChunkDescriptor> *newDesc);

            void ForeachChunkinRange(span<u8> memory, auto editCallback);

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

            size_t setHeapSize; //!< For use by svcSetHeapSize

            std::shared_mutex mutex; //!< Synchronizes any operations done on the VMM, it's locked in shared mode by readers and exclusive mode by writers

            MemoryManager(const DeviceState &state) noexcept;

            ~MemoryManager() noexcept;

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
             * @brief Sets the attributes for chunks within a certain range
             */
            void SetLockOnChunks(span<u8> memory, bool value);

            void SetCPUCachingOnChunks(span<u8> memory, bool value);

            /**
             * @brief Sets the permission for chunks within a certain range
             * @note The permissions set here are not accurate to the actual permissions set on the chunk and are only for the guest
             */
            void SetChunkPermission(span<u8> memory, memory::Permission permission);

            /**
             * @brief Gets the highest chunk's descriptor that contains this address
             */
            std::optional<std::pair<u8 *, ChunkDescriptor>> GetChunk(u8 *addr);

            /**
             * Various mapping functions for use by the guest
             * @note UnmapMemory frees the underlying memory as well
             */
            void MapCodeMemory(span<u8> memory, memory::Permission permission);

            void MapMutableCodeMemory(span<u8> memory);

            void MapStackMemory(span<u8> memory);

            void MapHeapMemory(span<u8> memory);

            void MapSharedMemory(span<u8> memory, memory::Permission permission);

            void MapTransferMemory(span<u8> memory, memory::Permission permission);

            void MapThreadLocalMemory(span<u8> memory);

            void Reserve(span<u8> memory);

            void UnmapMemory(span<u8> memory);

            /**
             * Frees the underlying memory
             * @note Memory that's not aligned to page boundaries at the edges of the span will not be freed
             */
            void FreeMemory(span<u8> memory);

            /**
             * Allows you to add/remove references to shared/transfer memory
             */
            void AddRef(const std::shared_ptr<type::KMemory> &ptr);

            void RemoveRef(const std::shared_ptr<type::KMemory> &ptr);

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
