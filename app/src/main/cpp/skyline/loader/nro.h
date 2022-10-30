// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "loader.h"

namespace skyline::loader {
    struct NroSegmentHeader {
        u32 offset;
        u32 size;
    };

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
        std::array<u64, 4> buildId; //!< The build ID of the NRO
        u64 _pad3_;

        NroSegmentHeader apiInfo; //!< The .apiInfo segment header
        NroSegmentHeader dynstr; //!< The .dynstr segment header
        NroSegmentHeader dynsym; //!< The .dynsym segment header
    };

    /**
     * @brief The NroLoader class abstracts access to an NRO file through the Loader interface
     * @url https://switchbrew.org/wiki/NRO
     */
    class NroLoader : public Loader {
      private:
        NroHeader header{};

        struct NroAssetSection {
            u64 offset;
            u64 size;
        };

        /**
         * @brief The asset section was created by homebrew developers to store additional data for their applications to use
         * @note This would actually be retrieved by NRO homebrew by reading the NRO file itself (reading its own binary) but libnx homebrew wrongly detects the images to be running in NSO mode where the RomFS is handled by HOS, this allows us to just provide the parsed data from the asset section to it directly
         */
        struct NroAssetHeader {
            u32 magic; //!< "ASET"
            u32 version;
            NroAssetSection icon;
            NroAssetSection nacp;
            NroAssetSection romFs;
        } assetHeader{};

        std::shared_ptr<vfs::Backing> backing;

        /**
         * @brief Reads the data of the specified segment
         * @param segment The header of the segment to read
         * @return A buffer containing the data of the requested segment
         */
        std::vector<u8> GetSegment(const NroSegmentHeader &segment);

      public:
        NroLoader(std::shared_ptr<vfs::Backing> backing);

        std::vector<u8> GetIcon(language::ApplicationLanguage language) override;

        void *LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) override;
    };
}
