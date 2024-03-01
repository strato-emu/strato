// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <crypto/key_store.h>
#include <vfs/partition_filesystem.h>
#include <vfs/rom_filesystem.h>
#include <vfs/nca.h>
#include "loader.h"

namespace skyline::loader {
    /**
     * @brief The XciLoader class abstracts access to an XCI file through the Loader interface
     * @url https://switchbrew.org/wiki/XCI
     */
    class XciLoader : public Loader {
      private:
        enum class GamecardSize : u8 {
            Size1GB = 0xFA,
            Size2GB = 0xF8,
            Size4GB = 0xF0,
            Size8GB = 0xE0,
            Size16GB = 0xE1,
            Size32GB = 0xE2
        };

        enum class GamecardFlags : u8 {
            AutoBoot = 0,
            HistoryErase = 1,
            RepairTool = 2,
            DifferentRegionCupToTerraDevice = 3,
            DifferentRegionCupToGlobalDevice = 4
        };

        /**
         * @brief The encryption type used for gamecard verification
         */
        enum class SecurityMode : u8 {
            T1 = 0x01,
            T2 = 0x02
        };

        enum class FirmwareVersion : u64 {
            Development = 0x00,
            Retail = 0x01,
            Retail400 = 0x02, //!< [4.0.0+] Retail
            Retail1100 = 0x04 //!< [11.0.0+] Retail
        };

        /**
         * @brief The speed at which the gamecard is accessed
         */
        enum class AccessControl : u32 {
            ClockRate25Mhz = 0x00A10011,
            ClockRate50Mhz = 0x00A10010
        };

        /**
         * @brief [9.0.0+] The region of Switch HW the gamecard is compatible with
         */
        enum class CompatType : u8 {
            Global = 0x00, //!< Normal
            China = 0x01 //!< Terra
        };

        struct GamecardInfo {
            FirmwareVersion firmwareVersion;
            AccessControl accessControl; //!< The speed at which the gamecard is accessed
            u32 readTimeWait1; //!< Read Time Wait1, always 0x1388
            u32 readTimeWait2; //!< Read Time Wait2, always 0
            u32 writeTimeWait1; //!< Write Time Wait1, always 0
            u32 writeTimeWait2; //!< Write Time Wait2, always 0
            u32 firmwareMode;
            u32 cupVersion;
            CompatType compatType;
            u8 _pad0_[0x3];
            u64 updatePartitionHash;
            u64 cupId; //!< CUP ID, always 0x0100000000000816, which is the title-listing data archive's title ID
            u8 _pad1_[0x38];
        };
        static_assert(sizeof(GamecardInfo) == 0x70);

        struct GamecardHeader {
            u8 signature[0x100]; //!< RSA-2048 PKCS #1 signature over the header
            u32 magic; //!< The magic of the gamecard format: 'HEAD'
            u32 secureAreaStartAddress; //!< Secure Area Start Address in media units
            u32 backupAreaStartAddress; //!< Backup Area Start Address, always 0xFFFFFFFF
            u8 titleKeyDecKekIndex; //!< TitleKeyDec Index (high nibble) and KEK Index (low nibble)
            GamecardSize size;
            u8 version; //!< Gamecard header version
            GamecardFlags flags; //!< GameCardAttribute
            u64 packageId; //!< The package ID, used for challenge–response authentication
            u64 validDataEndAddress; //!< Valid Data End Address in media units
            std::array<u8, 0x10> infoIv; //!< Gamecard Info IV (reversed)
            u64 hfs0PartitionOffset; //!< The HFS0 header partition offset
            u64 hfs0HeaderSize;
            std::array<u8, 0x20> hfs0HeaderSha256; //!< SHA-256 hash of the HFS0 Header
            std::array<u8, 0x20> initialDataSha256; //!< SHA-256 hash of the Initial Data
            SecurityMode securityMode;
            u32 t1KeyIndex; //!< T1 Key Index, always 2
            u32 keyIndex; //!< Key Index, always 0
            u32 normalAreaEndAddress; //!< Normal Area End Address in media units
            GamecardInfo gamecardInfo; //!< Gamecard Info (AES-128-CBC encrypted)
        } header{};
        static_assert(sizeof(GamecardHeader) == 0x200);

        std::shared_ptr<vfs::PartitionFileSystem> xci; //!< A shared pointer to XCI HFS0 header partition
        std::shared_ptr<vfs::PartitionFileSystem> secure; //!< A shared pointer to the secure HFS0 partition
        std::shared_ptr<vfs::PartitionFileSystem> update; //!< A shared pointer to the update HFS0 partition
        std::shared_ptr<vfs::PartitionFileSystem> normal; //!< A shared pointer to the normal HFS0 partition
        std::shared_ptr<vfs::PartitionFileSystem> logo; //!< A shared pointer to the logo HFS0 partition
        std::shared_ptr<vfs::RomFileSystem> controlRomFs; //!< A shared pointer to the control NCA's RomFS
        std::optional<vfs::NCA> programNca; //!< The main program NCA within the secure partition
        std::optional<vfs::NCA> controlNca; //!< The main control NCA within the secure partition
        std::optional<vfs::NCA> metaNca; //!< The main meta NCA within the secure partition

      public:
        XciLoader(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<crypto::KeyStore> &keyStore);

        std::vector<u8> GetIcon(language::ApplicationLanguage language) override;

        void *LoadProcessData(const std::shared_ptr<kernel::type::KProcess> &process, const DeviceState &state) override;
    };
}
