// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#pragma once

#include "filesystem.h"

namespace skyline::vfs {

    /**
     * @url https://switchbrew.org/wiki/NCM_services#ContentMetaType
     */
    enum class ContentMetaType : u8 {
        SystemProgram = 0x01,
        SystemData = 0x02,
        SystemUpdate = 0x03,
        BootImagePackage = 0x04,
        BootImagePackageSafe = 0x05,
        Application = 0x80,
        Patch = 0x81,
        AddOnContent = 0x82,
        Delta = 0x83,
        DataPatch = 0x84,
    };

    enum class ContentType : u8 {
        Meta = 0,
        Program = 1,
        Data = 2,
        Control = 3,
        HtmlDocument = 4,
        LegalInformation = 5,
        DeltaFragment = 6,
    };

    /**
     * @url https://switchbrew.org/wiki/CNMT#PackagedContentMetaHeader
     */
    struct PackagedContentMetaHeader {
        u64 id;
        u32 version;
        ContentMetaType contentMetaType;
        u8 _pad0_;
        u16 extendedHeaderSize;
        u16 contentCount;
        u16 contentMetaCount;
        u8 contentMetaAttributes;
        u8 _pad1_[0x3];
        u32 requiredDownloadSystemVersion;
        u8 _pad2_[0x4];
    };
    static_assert(sizeof(PackagedContentMetaHeader) == 0x20);

    /**
     * @url https://switchbrew.org/wiki/CNMT#PackagedContentInfo
     */
    struct PackagedContentInfo {
        std::array<u8, 0x20> hash;
        std::array<u8, 0x10> contentId;
        std::array<u8, 0x6> size;
        ContentType contentType;
        u8 idOffset;
    };
    static_assert(sizeof(PackagedContentInfo) == 0x38);

    /**
     * @url https://switchbrew.org/wiki/CNMT#ContentMetaInfo
     */
    struct ContentMetaInfo {
        u64 id;
        u32 version;
        ContentMetaType contentMetaType;
        u8 contentMetaAttributes;
        u8 _pad0_[0x2];
    };
    static_assert(sizeof(ContentMetaInfo) == 0x10);

    struct OptionalHeader {
        u64 titleId;
        u64 minimumVersion;
    };
    static_assert(sizeof(OptionalHeader) == 0x10);

    /**
     * @brief The CNMT class provides easy access to the data found in an CNMT file
     * @url https://switchbrew.org/wiki/CNMT
     */
    class CNMT {
      private:
        OptionalHeader optionalHeader;
        std::vector<PackagedContentInfo> contentInfos;
        std::vector<ContentMetaInfo> contentMetaInfos;

      public:
        PackagedContentMetaHeader header;

        CNMT(std::shared_ptr<FileSystem> file);

        std::string GetTitleId();

        std::string GetParentTitleId();

        ContentMetaType GetContentMetaType();
    };

}
