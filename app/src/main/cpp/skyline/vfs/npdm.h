// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "backing.h"
#include <kernel/scheduler.h>
#include <kernel/memory.h>

namespace skyline {
    namespace vfs {
        /**
         * @url https://switchbrew.org/wiki/NPDM
         */
        class NPDM {
          private:
            struct Section {
                u32 offset;
                u32 size;

                template<class T>
                inline T Read(const std::shared_ptr<vfs::Backing> &backing, size_t baseOffset = 0) {
                    if (sizeof(T) > size)
                        throw exception("Section size ({}) smaller than Read type size ({})", size, sizeof(T));
                    return backing->Read<T>(baseOffset + offset);
                }
            };
            static_assert(sizeof(Section) == sizeof(u64));

          public:
            /**
             * @url https://switchbrew.org/wiki/NPDM#META
             */
            struct __attribute__((packed)) NpdmMeta {
                u32 magic; //!< "META"
                u32 acidSignatureKeyGeneration;
                u32 _unk0_;
                union {
                    struct {
                        bool is64Bit : 1;
                        memory::AddressSpaceType type : 2;
                        bool optimizeMemoryAllocation : 1;
                    };
                    u8 raw{};
                } flags;
                u8 _unk1_;
                u8 mainThreadPriority;
                u8 idealCore;
                u32 _unk2_;
                u32 systemResourceSize; //!< 3.0.0+
                u32 version;
                u32 mainThreadStackSize;
                std::array<char, 0x10> name; //!< "Application"
                std::array<u8, 0x10> productCode;
                u8 _unk3_[0x30];
                Section aci0;
                Section acid;
            } meta;
            static_assert(sizeof(NpdmMeta::flags) == sizeof(u8));
            static_assert(sizeof(NpdmMeta) == 0x80);

            /**
             * @url https://switchbrew.org/wiki/NPDM#ACI0
             * @note Offsets in this are all relative to ACI0, not the start of the file
             */
            struct __attribute__((packed)) NpdmAci0 {
                u32 magic; //!< "ACI0"
                u32 _res0_[3];
                u64 programId;
                u64 _res1_;
                Section fsAccessControl;
                Section srvAccessControl;
                Section kernelCapability;
                u64 _res2_;
            } aci0;
            static_assert(sizeof(NpdmAci0) == 0x40);

            /**
             * @url https://switchbrew.org/wiki/NPDM#KernelCapability
             */
            union __attribute__((packed)) NpdmKernelCapability {
                /**
                 * @url https://switchbrew.org/wiki/NPDM#ThreadInfo
                 * @note Priority field names are based on real scheduler priority (Lower value is higher priority)
                 */
                struct __attribute__((packed)) {
                    u8 pattern : 4; //!< 0b0111
                    u8 lowestPriority : 6;
                    u8 highestPriority : 6;
                    u8 minCoreId : 8;
                    u8 maxCoreId : 8;
                } threadInfo;

                /**
                 * @url https://switchbrew.org/wiki/NPDM#KernelVersion
                 */
                struct __attribute__((packed)) {
                    u16 pattern : 15; //!< 0b011111111111111
                    u8 minorVersion : 4;
                    u16 majorVersion : 13;
                } kernelVersion;

                u32 raw;
            };
            static_assert(sizeof(NpdmKernelCapability) == sizeof(u32));

            struct {
                kernel::Priority priority;
                kernel::CoreMask coreMask;
            } threadInfo;

            struct {
                u8 minorVersion;
                u16 majorVersion;
            } kernelVersion{};

          public:
            NPDM();

            NPDM(const std::shared_ptr<vfs::Backing> &backing, const DeviceState &state);
        };
    }
}
