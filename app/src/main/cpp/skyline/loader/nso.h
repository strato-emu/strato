// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "loader.h"

namespace skyline::loader {
    /**
     * @brief The NsoLoader class abstracts access to an NSO file through the Loader interface
     * @url https://switchbrew.org/wiki/NSO
     */
    class NsoLoader : public Loader {
      private:
        std::shared_ptr<vfs::Backing> backing;

        union NsoFlags {
            struct {
                bool textCompressed : 1; //!< .text is compressed
                bool roCompressed : 1; //!< .rodata is compressed
                bool dataCompressed : 1; //!< .data is compressed
                bool textHash : 1; //!< .text hash should be checked before loading
                bool roHash : 1; //!< .rodata hash should be checked before loading
                bool dataHash : 1; //!< .data hash should be checked before loading
            };
            u32 raw;
        };
        static_assert(sizeof(NsoFlags) == 0x4);

        struct NsoSegmentHeader {
            u32 fileOffset; //!< The offset of the segment in the NSO
            u32 memoryOffset; //!< The memory offset where the region should be loaded
            u32 decompressedSize; //!< Size of the region after decompression
        };
        static_assert(sizeof(NsoSegmentHeader) == 0xC);

        struct NsoHeader {
            u32 magic; //!< The NSO magic "NSO0"
            u32 version; //!< The version of the application
            u32 _pad0_;
            NsoFlags flags; //!< The flags used with the NSO

            NsoSegmentHeader text; //!< The .text segment header
            u32 modOffset; //!< The offset of the MOD metadata
            NsoSegmentHeader ro; //!< The .rodata segment header
            u32 modSize; //!< The size of the MOD metadata
            NsoSegmentHeader data; //!< The .data segment header
            u32 bssSize; //!< The size of the .bss segment

            std::array<u64, 4> buildId; //!< The build ID of the NSO

            u32 textCompressedSize; //!< The size of the compressed .text segment
            u32 roCompressedSize; //!< The size of the compressed .rodata segment
            u32 dataCompressedSize; //!< The size of the compressed .data segment

            u32 _pad1_[7];

            u64 apiInfo; //!< The .rodata-relative offset of .apiInfo
            u64 dynstr; //!< The .rodata-relative offset of .dynstr
            u64 dynsym; //!< The .rodata-relative offset of .dynsym

            std::array<std::array<u64, 4>, 3> segmentHashes; //!< The SHA256 checksums of the .text, .rodata and .data segments
        };
        static_assert(sizeof(NsoHeader) == 0x100);

        /**
         * @brief Reads the specified segment from the backing and decompresses it if needed
         * @param segment The header of the segment to read
         * @param compressedSize The compressed size of the segment, 0 if the segment is not compressed
         * @return A buffer containing the data of the requested segment
         */
        static std::vector<u8> GetSegment(const std::shared_ptr<vfs::Backing> &backing, const NsoSegmentHeader &segment, u32 compressedSize);

      public:
        NsoLoader(const std::shared_ptr<vfs::Backing> &backing);

        /**
         * @brief Loads an NSO into memory, offset by the given amount
         * @param backing The backing of the NSO
         * @param process The process to load the NSO into
         * @param offset The offset from the base address to place the NSO
         * @return An ExecutableLoadInfo struct containing the load base and size
         */
        static ExecutableLoadInfo LoadNso(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state, size_t offset = 0);

        void* LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state);
    };
}
