// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "loader.h"

namespace skyline::loader {
    /**
     * @brief The NroLoader class abstracts access to an NRO file through the Loader interface (https://switchbrew.org/wiki/NRO)
     */
    class NroLoader : public Loader {
      private:
        /**
         * @brief This holds a single data segment's offset and size
         */
        struct NroSegmentHeader {
            u32 offset; //!< The offset of the region
            u32 size; //!< The size of the region
        };

        /**
         * @brief This holds the header of an NRO file
         */
        struct NroHeader {
            u32 _pad0_;
            u32 modOffset; //!< The offset of the MOD metadata
            u64 _pad1_;

            u32 magic; //!< The NRO magic "NRO0"
            u32 version; //!< The version of the application
            u32 size; //!< The size of the NRO
            u32 flags; //!< The flags used with the NRO

            NroSegmentHeader text; //!< The .text segment header
            NroSegmentHeader ro; //!< The .rodata segment header
            NroSegmentHeader data; //!< The .data segment header

            u32 bssSize; //!< The size of the bss segment
            u32 _pad2_;
            u64 buildId[4]; //!< The build ID of the NRO
            u64 _pad3_;

            NroSegmentHeader apiInfo; //!< The .apiInfo segment header
            NroSegmentHeader dynstr; //!< The .dynstr segment header
            NroSegmentHeader dynsym; //!< The .dynsym segment header
        } header{};

        /**
         * @brief This holds a single asset section's offset and size
         */
        struct NroAssetSection {
            u64 offset; //!< The offset of the region
            u64 size; //!< The size of the region
        };

        /**
        * @brief This holds various metadata about an NRO, it is only used by homebrew
        */
        struct NroAssetHeader {
            u32 magic; //!< The asset section magic "ASET"
            u32 version; //!< The format version
            NroAssetSection icon; //!< The header describing the location of the icon
            NroAssetSection nacp; //!< The header describing the location of the NACP
            NroAssetSection romFs; //!< The header describing the location of the RomFS
        } assetHeader{};

        std::shared_ptr<vfs::Backing> backing; //!< The backing of the NRO loader

        /**
         * @brief This reads the data of the specified segment
         * @param segment The header of the segment to read
         * @return A buffer containing the data of the requested segment
         */
        std::vector<u8> GetSegment(const NroSegmentHeader &segment);

      public:
        NroLoader(const std::shared_ptr<vfs::Backing> &backing);

        std::vector<u8> GetIcon();

        void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state);
    };
}
