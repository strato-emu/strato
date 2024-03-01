// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <crypto/key_store.h>
#include <crypto/aes_cipher.h>
#include "filesystem.h"

namespace skyline {
    namespace constant {
        constexpr size_t MediaUnitSize{0x200}; //!< The unit size of entries in an NCA
        constexpr size_t IvfcMaxLevel{6};
        constexpr u64 SectionHeaderSize{0x200};
        constexpr u64 SectionHeaderOffset{0x400};
    }

    namespace vfs {
        enum class NCAContentType : u8 {
            Program = 0x0,    //!< Program NCA
            Meta = 0x1,       //!< Metadata NCA
            Control = 0x2,    //!< Control NCA
            Manual = 0x3,     //!< Manual NCA
            Data = 0x4,       //!< Data NCA
            PublicData = 0x5, //!< Public data NCA
        };

        enum class NcaDistributionType : u8 {
            System = 0x0, //!< This NCA was distributed on the EShop or is part of the system
            GameCard = 0x1, //!< This NCA was distributed on a GameCard
        };

        /**
         * @brief The key generation version in NCAs before HOS 3.0.1
         */
        enum class NcaLegacyKeyGenerationType : u8 {
            Fw100 = 0x0, //!< 1.0.0
            Fw300 = 0x2, //!< 3.0.0
        };

        /**
         * @brief The key generation version in NCAs after HOS 3.0.0, this is changed by Nintendo frequently
         */
        enum class NcaKeyGenerationType : u8 {
            Fw301 = 0x3, //!< 3.0.1
            Fw400 = 0x4, //!< 4.0.0
            Fw500 = 0x5, //!< 5.0.0
            Fw600 = 0x6, //!< 6.0.0
            Fw620 = 0x7, //!< 6.2.0
            Fw700 = 0x8, //!< 7.0.0
            Fw810 = 0x9, //!< 8.1.0
            Fw900 = 0xA, //!< 9.0.0
            Fw910 = 0xB, //!< 9.1.0
            Invalid = 0xFF, //!< An invalid key generation type
        };

        enum class NCAKeyAreaEncryptionKeyType : u8 {
            Application = 0x0, //!< This NCA uses the application key encryption area
            Ocean = 0x1, //!< This NCA uses the ocean key encryption area
            System = 0x2, //!< This NCA uses the system key encryption area
        };

        struct NcaFsEntry {
            u32 startOffset; //!< The start offset of the filesystem in units of 0x200 bytes
            u32 endOffset; //!< The start offset of the filesystem in units of 0x200 bytes
            u64 _pad_;
        };

        enum class NcaSectionFsType : u8 {
            PFS0 = 0x2, //!< This section contains a PFS0 filesystem
            RomFs = 0x3, //!< This section contains a RomFs filesystem
        };

        enum class NcaSectionHashType : u8 {
            HierarchicalSha256 = 0x2, //!< The hash header for this section is that of a PFS0
            HierarchicalIntegrity = 0x3, //!< The hash header for this section is that of a RomFS
        };

        enum class NcaSectionEncryptionType : u8 {
            None = 0x1, //!< This NCA doesn't use any encryption
            XTS = 0x2, //!< This NCA uses AES-XTS encryption
            CTR = 0x3, //!< This NCA uses AES-CTR encryption
            BKTR = 0x4, //!< This NCA uses BKTR together AES-CTR encryption
        };

        /**
         * @brief The data for a single level of the hierarchical integrity scheme
         */
        struct HierarchicalIntegrityLevel {
            u64 offset; //!< The offset of the level data
            u64 size; //!< The size of the level data
            u32 blockSize; //!< The block size of the level data
            u32 _pad_;
        };
        static_assert(sizeof(HierarchicalIntegrityLevel) == 0x18);

        struct NCASectionHeaderBlock {
            u8 _pad0_[0x3];
            NcaSectionFsType fsType;
            NcaSectionEncryptionType encryptionType;
            u8 _pad1_[0x3];
        };
        static_assert(sizeof(NCASectionHeaderBlock) == 0x8);

        struct PFS0Superblock {
            NCASectionHeaderBlock headerBlock;
            std::array<u8, 0x20> hash;
            u32 size;
            u8 _pad0_[0x4];
            u64 hashTableOffset;
            u64 hashTableSize;
            u64 pfs0HeaderOffset;
            u64 pfs0Size;
            u8 _pad1_[0x1B0];
        };
        static_assert(sizeof(PFS0Superblock) == 0x200);

        /**
         * @brief The hash info header of the SHA256 hashing scheme for PFS0
         */
        struct HierarchicalSha256HashInfo {
            std::array<u8, 0x20> hashTableHash; //!< A SHA256 hash over the hash table
            u32 blockSize; //!< The block size of the filesystem
            u32 _pad_;
            u64 hashTableOffset; //!< The offset from the end of the section header of the hash table
            u64 hashTableSize; //!< The size of the hash table
            u64 pfs0Offset; //!< The offset from the end of the section header of the PFS0
            u64 pfs0Size; //!< The size of the PFS0
            u8 _pad1_[0xB0];
        };
        static_assert(sizeof(HierarchicalSha256HashInfo) == 0xF8);

        struct NCABucketInfo {
            u64 tableOffset;
            u64 tableSize;
            std::array<u8, 0x10> tableHeader;
        };
        static_assert(sizeof(NCABucketInfo) == 0x20);

        struct NCASparseInfo {
            NCABucketInfo bucket;
            u64 physicalOffset;
            u16 generation;
            u8 _pad0_[0x6];
        };
        static_assert(sizeof(NCASparseInfo) == 0x30);

        struct NCACompressionInfo {
            NCABucketInfo bucket;
            u8 _pad0_[0x8];
        };
        static_assert(sizeof(NCACompressionInfo) == 0x28);

        struct NCASectionRaw {
            NCASectionHeaderBlock header;
            std::array<u8, 0x138> blockData;
            std::array<u8, 0x8> sectionCtr;
            NCASparseInfo sparseInfo;
            NCACompressionInfo compressionInfo;
            u8 _pad0_[0x60];
        };
        static_assert(sizeof(NCASectionRaw) == 0x200);

        struct IVFCLevel {
            u64 offset;
            u64 size;
            u32 blockSize;
            u32 reserved;
        };
        static_assert(sizeof(IVFCLevel) == 0x18);

        struct IVFCHeader {
            u32 magic;
            u32 magicNumber;
            u8 _pad0_[0x8];
            std::array<IVFCLevel, 6> levels;
            u8 _pad1_[0x40];
        };
        static_assert(sizeof(IVFCHeader) == 0xE0);

        struct RomFSSuperblock {
            NCASectionHeaderBlock headerBlock;
            IVFCHeader ivfc;
            u8 _pad0_[0x118];
        };
        static_assert(sizeof(RomFSSuperblock) == 0x200);

        struct BKTRHeader {
            u64 offset;
            u64 size;
            u32 magic;
            u8 _pad0_[0x4];
            u32 numberEntries;
            u8 _pad1_[0x4];
        };
        static_assert(sizeof(BKTRHeader) == 0x20);

        struct BKTRSuperblock {
            NCASectionHeaderBlock headerBlock;
            IVFCHeader ivfc;
            u8 _pad0_[0x18];
            BKTRHeader relocation;
            BKTRHeader subsection;
            u8 _pad1_[0xC0];
        };
        static_assert(sizeof(BKTRSuperblock) == 0x200);

        union NCASectionHeader {
            NCASectionRaw raw{};
            PFS0Superblock pfs0;
            RomFSSuperblock romfs;
            BKTRSuperblock bktr;
        };
        static_assert(sizeof(NCASectionHeader) == 0x200);

        struct RelocationBlock {
            u8 _pad0_[0x4];
            u32 numberBuckets;
            u64 size;
            std::array<u64, 0x7FE> baseOffsets;
        };
        static_assert(sizeof(RelocationBlock) == 0x4000);

        #pragma pack(push, 1)
        struct RelocationEntry {
            u64 addressPatch;
            u64 addressSource;
            u32 fromPatch;
        };
        #pragma pack(pop)
        static_assert(sizeof(RelocationEntry) == 0x14);

        struct SubsectionBlock {
            u8 _pad0_[0x4];
            u32 numberBuckets;
            u64 size;
            std::array<u64, 0x7FE> baseOffsets;
        };
        static_assert(sizeof(SubsectionBlock) == 0x4000);

        struct SubsectionEntry {
            u64 addressPatch;
            u8 _pad0_[0x4];
            u32 ctr;
        };
        static_assert(sizeof(SubsectionEntry) == 0x10);

        struct NCASectionTableEntry {
            u32 mediaOffset;
            u32 mediaEndOffset;
            u8 _pad0_[0x8];
        };
        static_assert(sizeof(NCASectionTableEntry) == 0x10);

        struct NCAHeader {
            std::array<u8, 0x100> rsaSignature1;
            std::array<u8, 0x100> rsaSignature2;
            u32 magic;
            u8 isSystem;
            NCAContentType contentType;
            u8 cryptoType;
            NCAKeyAreaEncryptionKeyType keyIndex;
            u64 size;
            u64 titleId;
            u8 _pad0_[0x4];
            u32 sdkVersion;
            u8 cryptoType2;
            u8 _pad1_[0xF];
            std::array<u8, 0x10> rightsId;
            std::array<NCASectionTableEntry, 0x4> sectionTables;
            std::array<std::array<u8, 0x20>, 0x4> hashTables;
            std::array<std::array<u8, 0x10>, 4> keyArea;
            u8 _pad2_[0xC0];
        };
        static_assert(sizeof(NCAHeader) == 0x400);

        struct RelocationBucketRaw {
            u8 _pad0_[0x4];
            u32 numberEntries;
            u64 endOffset;
            std::array<RelocationEntry, 0x332> relocationEntries;
            u8 _pad1_[0x8];
        };
        static_assert(sizeof(RelocationBucketRaw) == 0x4000);

        struct RelocationBucket {
            u32 numberEntries;
            u64 endOffset;
            std::vector<RelocationEntry> entries;
        };

        struct SubsectionBucket {
            u32 numberEntries;
            u64 endOffset;
            std::vector<SubsectionEntry> entries;
        };

        struct SubsectionBucketRaw {
            u8 _pad0_[0x4];
            u32 numberEntries;
            u64 endOffset;
            std::array<SubsectionEntry, 0x3FF> subsectionEntries;
        };
        static_assert(sizeof(SubsectionBucketRaw) == 0x4000);

        /**
         * @brief The NCA class provides an easy way to access the contents of an Nintendo Content Archive
         * @url https://switchbrew.org/wiki/NCA_Format
         */
        class NCA {
          private:
            std::shared_ptr<Backing> backing;
            std::shared_ptr<crypto::KeyStore> keyStore;
            bool encrypted{false};
            bool rightsIdEmpty;
            bool useKeyArea;
            std::vector<std::shared_ptr<Backing>> files;
            std::vector<NCASectionHeader> sections;
            std::shared_ptr<vfs::Backing> bktrBaseRomfs;
            u64 bktrBaseIvfcOffset;

            void ReadPfs0(const NCASectionHeader &sectionHeader, const NCASectionTableEntry &entry);

            void ReadRomFs(const NCASectionHeader &sectionHeader, const NCASectionTableEntry &entry);

            std::shared_ptr<Backing> CreateBacking(const NCASectionHeader &sectionHeader, std::shared_ptr<Backing> rawBacking, size_t offset);

            u8 GetKeyGeneration();

            crypto::KeyStore::Key128 GetTitleKey();

            crypto::KeyStore::Key128 GetKeyAreaKey(NcaSectionEncryptionType type);

            void ValidateNCA(const NCASectionHeader &sectionHeader);

            static RelocationBucket ConvertRelocationBucketRaw(RelocationBucketRaw raw) {
                return {raw.numberEntries, raw.endOffset, {raw.relocationEntries.begin(), raw.relocationEntries.begin() + raw.numberEntries}};
            }

            static SubsectionBucket ConvertSubsectionBucketRaw(SubsectionBucketRaw raw) {
                return {raw.numberEntries, raw.endOffset, {raw.subsectionEntries.begin(), raw.subsectionEntries.begin() + raw.numberEntries}};
            }

          public:
            std::shared_ptr<FileSystem> exeFs; //!< The PFS0 filesystem for this NCA's ExeFS section
            std::shared_ptr<FileSystem> logo; //!< The PFS0 filesystem for this NCA's logo section
            std::shared_ptr<FileSystem> cnmt; //!< The PFS0 filesystem for this NCA's CNMT section
            std::shared_ptr<Backing> romFs; //!< The backing for this NCA's RomFS section
            NCAHeader header; //!< The header of the NCA
            NCAContentType contentType; //!< The content type of the NCA
            u64 ivfcOffset{0};

            NCA(std::shared_ptr<vfs::Backing> backing, std::shared_ptr<crypto::KeyStore> keyStore, bool useKeyArea = false);

            NCA(std::optional<vfs::NCA> updateNca, std::shared_ptr<crypto::KeyStore> pKeyStore, std::shared_ptr<vfs::Backing> bktrBaseRomfs,
                     u64 bktrBaseIvfcOffset, bool useKeyArea = false);
        };
    }
}
