// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include "filesystem.h"

namespace skyline {
    namespace constant {
        constexpr size_t MediaUnitSize = 0x200; //!< The unit size of entries in an NCA
    }

    namespace vfs {
        /**
         * @brief This enumerates the various content types of an NCA
         */
        enum class NcaContentType : u8 {
            Program = 0x0, //!< This is a program NCA
            Meta = 0x1, //!< This is a metadata NCA
            Control = 0x2, //!< This is a control NCA
            Manual = 0x3, //!< This is a manual NCA
            Data = 0x4, //!< This is a data NCA
            PublicData = 0x5, //!< This is a public data NCA
        };

        /**
         * @brief The NCA class provides an easy way to access the contents of an NCA file (https://switchbrew.org/wiki/NCA_Format)
         */
        class NCA {
          private:
            /**
             * @brief This enumerates the distribution types of an NCA
             */
            enum class NcaDistributionType : u8 {
                System = 0x0, //!< This NCA was distributed on the EShop or is part of the system
                GameCard = 0x1, //!< This NCA was distributed on a GameCard
            };

            /**
             * @brief This enumerates the key generation version in NCAs before HOS 3.0.1
             */
            enum class NcaLegacyKeyGenerationType : u8 {
                Fw100 = 0x0, //!< 1.0.0
                Fw300 = 0x2, //!< 3.0.0
            };

            /**
             * @brief This enumerates the key generation version in NCAs after HOS 3.0.0
             */
            enum class NcaKeyGenerationType : u8 {
                Fw301 = 0x3, //!< 3.0.1
                Fw400 = 0x4, //!< 4.0.0
                Fw500 = 0x5, //!< 5.0.0
                Fw600 = 0x6, //!< 6.0.0
                Fw620 = 0x7, //!< 6.2.0
                Fw700 = 0x8, //!< 7.0.0
                Fw810 = 0x9, //!< 8.1.0
                Fw900 = 0xa, //!< 9.0.0
                Fw910 = 0xb, //!< 9.1.0
                Invalid = 0xff, //!< An invalid key generation type
            };

            /**
             * @brief This enumerates the key area encryption key types
             */
            enum class NcaKeyAreaEncryptionKeyType : u8 {
                Application = 0x0, //!< This NCA uses the application key encryption area
                Ocean = 0x1, //!< This NCA uses the ocean key encryption area
                System = 0x2, //!< This NCA uses the system key encryption area
            };

            /**
             * @brief This hold the entry of a single filesystem in an NCA
             */
            struct NcaFsEntry {
                u32 startOffset; //!< The start offset of the filesystem in units of 0x200 bytes
                u32 endOffset; //!< The start offset of the filesystem in units of 0x200 bytes
                u64 _pad_;
            };

            /**
             * @brief This enumerates the possible filesystem types a section of an NCA can contain
             */
            enum class NcaSectionFsType : u8 {
                RomFs = 0x0, //!< This section contains a RomFs filesystem
                PFS0 = 0x1, //!< This section contains a PFS0 filesystem
            };

            /**
             * @brief This enumerates the possible hash header types of an NCA section
             */
            enum class NcaSectionHashType : u8 {
                HierarchicalSha256 = 0x2, //!< The hash header for this section is that of a PFS0
                HierarchicalIntegrity = 0x3, //!< The hash header for this section is that of a RomFS
            };

            /**
             * @brief This enumerates the possible encryption types of an NCA section
             */
            enum class NcaSectionEncryptionType : u8 {
                None = 0x1, //!< This NCA doesn't use any encryption
                XTS = 0x2, //!< This NCA uses AES-XTS encryption
                CTR = 0x3, //!< This NCA uses AES-CTR encryption
                BKTR = 0x4, //!< This NCA uses BKTR together AES-CTR encryption
            };

            /**
             * @brief This holds the data for a single level of the hierarchical integrity scheme
             */
            struct HierarchicalIntegrityLevel {
                u64 offset; //!< The offset of the level data
                u64 size; //!< The size of the level data
                u32 blockSize; //!< The block size of the level data
                u32 _pad_;
            };
            static_assert(sizeof(HierarchicalIntegrityLevel) == 0x18);

            /**
             * @brief This holds the hash info header of the hierarchical integrity scheme
             */
            struct HierarchicalIntegrityHashInfo {
                u32 magic; //!< The hierarchical integrity magic, 'IVFC'
                u32 magicNumber; //!< The magic number 0x2000
                u32 masterHashSize; //!< The size of the master hash
                u32 numLevels; //!< The number of levels
                std::array<HierarchicalIntegrityLevel, 6> levels; //!< An array of the hierarchical integrity levels
                u8 _pad0_[0x20];
                std::array<u8, 0x20> masterHash; //!< The master hash of the hierarchical integrity system
                u8 _pad1_[0x18];
            };
            static_assert(sizeof(HierarchicalIntegrityHashInfo) == 0xf8);

            /**
             * @brief This holds the hash info header of the SHA256 hashing scheme for PFS0
             */
            struct HierarchicalSha256HashInfo {
                std::array<u8, 0x20> hashTableHash; //!< A SHA256 hash over the hash table
                u32 blockSize; //!< The block size of the filesystem
                u32 _pad_;
                u64 hashTableOffset; //!< The offset from the end of the section header of the hash table
                u64 hashTableSize; //!< The size of the hash table
                u64 pfs0Offset; //!< The offset from the end of the section header of the PFS0
                u64 pfs0Size; //!< The size of the PFS0
                u8 _pad1_[0xb0];
            };
            static_assert(sizeof(HierarchicalSha256HashInfo) == 0xf8);

            /**
             * @brief This holds the header of each specific section in an NCA
             */
            struct NcaSectionHeader {
                u16 version; //!< The version, always 2
                NcaSectionFsType fsType; //!< The type of the filesystem in the section
                NcaSectionHashType hashType; //!< The type of hash header that is used for this section
                NcaSectionEncryptionType encryptionType; //!< The type of encryption that is used for this section
                u8 _pad0_[0x3];
                union {
                    HierarchicalIntegrityHashInfo integrityHashInfo; //!< The HashInfo used for RomFS
                    HierarchicalSha256HashInfo sha256HashInfo; //!< The HashInfo used for PFS0
                };
                u8 _pad1_[0x40]; // PatchInfo
                u32 generation; //!< The generation of the NCA section
                u32 secureValue; //!< The secure value of the section
                u8 _pad2_[0x30]; //!< SparseInfo
                u8 _pad3_[0x88];
            };
            static_assert(sizeof(NcaSectionHeader) == 0x200);

            /**
             * @brief This struct holds the header of a Nintendo Content Archive
             */
            struct NcaHeader {
                std::array<u8, 0x100> fixed_key_sig; //!< An RSA-PSS signature over the header with fixed key
                std::array<u8, 0x100> npdm_key_sig; //!< An RSA-PSS signature over header with key in NPDM
                u32 magic; //!< The magic of the NCA: 'NCA3'
                NcaDistributionType distributionType; //!< Whether this NCA is from a gamecard or the E-Shop
                NcaContentType contentType; //!< The content type of the NCA
                NcaLegacyKeyGenerationType legacyKeyGenerationType; //!< The keyblob to use for decryption
                NcaKeyAreaEncryptionKeyType keyAreaEncryptionKeyType; //!< The index of the key area encryption key that is needed
                u64 size; //!< The total size of the NCA
                u64 programId; //!< The program ID of the NCA
                u32 contentIndex; //!< The index of the content
                u32 sdkVersion; //!< The version of the SDK the NCA was built with
                NcaKeyGenerationType keyGenerationType; //!< The keyblob to use for decryption
                u8 fixedKeyGeneration; //!< The fixed key index
                u8 _pad0_[0xe];
                std::array<u8, 0x10> rightsId; //!< The NCA's rights ID
                std::array<NcaFsEntry, 4> fsEntries; //!< The filesystem entries for this NCA
                std::array<std::array<u8, 0x20>, 4> sectionHashes; //!< This contains SHA-256 hashes for each filesystem header
                std::array<std::array<u8, 0x10>, 4> encryptedKeyArea; //!< The encrypted key area for each filesystem
                u8 _pad1_[0xc0];
                std::array<NcaSectionHeader, 4> sectionHeaders;
            } header{};
            static_assert(sizeof(NcaHeader) == 0xc00);

            std::shared_ptr<Backing> backing; //!< The backing for the NCA

            void ReadPfs0(const NcaSectionHeader &header, const NcaFsEntry &entry);

            void ReadRomFs(const NcaSectionHeader &header, const NcaFsEntry &entry);

          public:
            std::shared_ptr<FileSystem> exeFs; //!< The PFS0 filesystem for this NCA's ExeFS section
            std::shared_ptr<FileSystem> logo; //!< The PFS0 filesystem for this NCA's logo section
            std::shared_ptr<FileSystem> cnmt; //!< The PFS0 filesystem for this NCA's CNMT section
            std::shared_ptr<Backing> romFs; //!< The backing for this NCA's RomFS section
            NcaContentType contentType; //!< The content type of the NCA

            NCA(const std::shared_ptr<vfs::Backing> &backing);
        };
    }
}