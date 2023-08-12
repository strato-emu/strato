// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)

#include "bktr.h"
#include "region_backing.h"

namespace skyline::vfs {
    template <typename BlockType, typename BucketType>
    std::pair<u64, u64> SearchBucketEntry(u64 offset, const BlockType &block, const BucketType &buckets, bool isSubsection) {
        if (isSubsection) {
            const auto &lastBucket{buckets[block.numberBuckets - 1]};
            if (offset >= lastBucket.entries[lastBucket.numberEntries].addressPatch) {
                return {block.numberBuckets - 1, lastBucket.numberEntries};
            }
        }

        u64 bucketId{static_cast<u64>(std::distance(block.baseOffsets.begin(),
                                                    std::upper_bound(block.baseOffsets.begin() + 1,
                                                                     block.baseOffsets.begin() + block.numberBuckets, offset)) - 1)};

        const auto &bucket{buckets[bucketId]};

        if (bucket.numberEntries == 1)
            return {bucketId, 0};

        auto entryIt{std::upper_bound(bucket.entries.begin(), bucket.entries.begin() + bucket.numberEntries, offset, [](u64 offset, const auto &entry) {
            return offset < entry.addressPatch;
        })};

        if (entryIt != bucket.entries.begin()) {
            u64 entryIndex{static_cast<u64>(std::distance(bucket.entries.begin(), entryIt) - 1)};
            return {bucketId, entryIndex};
        }
        LOGE("Offset could not be found.");
        return {0, 0};
    }

    BKTR::BKTR(std::shared_ptr<vfs::Backing> pBaseRomfs, std::shared_ptr<vfs::Backing> pBktrRomfs, RelocationBlock pRelocation,
               std::vector<RelocationBucket> pRelocationBuckets, SubsectionBlock pSubsection,
               std::vector<SubsectionBucket> pSubsectionBuckets, bool pIsEncrypted, std::array<u8, 16> pKey,
               u64 pBaseOffset, u64 pIvfcOffset, std::array<u8, 8> pSectionCtr)
               : baseRomFs(std::move(pBaseRomfs)), bktrRomFs(std::move(pBktrRomfs)),
                 relocation(pRelocation), relocationBuckets(std::move(pRelocationBuckets)),
                 subsection(pSubsection), subsectionBuckets(std::move(pSubsectionBuckets)),
                 isEncrypted(pIsEncrypted), key(pKey), baseOffset(pBaseOffset), ivfcOffset(pIvfcOffset),
                 sectionCtr(pSectionCtr) {

        for (std::size_t i = 0; i < relocation.numberBuckets - 1; ++i)
            relocationBuckets[i].entries.push_back({relocation.baseOffsets[i + 1], 0, 0});

        for (std::size_t i = 0; i < subsection.numberBuckets - 1; ++i)
            subsectionBuckets[i].entries.push_back({subsectionBuckets[i + 1].entries[0].addressPatch, {0}, subsectionBuckets[i + 1].entries[0].ctr});

        relocationBuckets.back().entries.push_back({relocation.size, 0, 0});
    }

    size_t BKTR::ReadImpl(span<u8> output, size_t offset) {
        if (offset >= relocation.size)
            return 0;

        const auto relocationEntry{GetRelocationEntry(offset)};
        const auto sectionOffset{offset - relocationEntry.addressPatch + relocationEntry.addressSource};

        const auto nextRelocation{GetNextRelocationEntry(offset)};

       if (offset + output.size() > nextRelocation.addressPatch) {
           const u64 partition{nextRelocation.addressPatch - offset};
           span<u8> data(output.data() + partition, output.size() - partition);
           return ReadWithPartition(data, output.size() - partition, offset + partition) + ReadWithPartition(output, partition, offset);
       }

       if (!relocationEntry.fromPatch) {
           auto regionBacking{std::make_shared<RegionBacking>(baseRomFs, sectionOffset - ivfcOffset, output.size())};
           return regionBacking->Read(output);
       }

        if (!isEncrypted)
            return bktrRomFs->Read(output, sectionOffset);

        const auto subsectionEntry{GetSubsectionEntry(sectionOffset)};

        crypto::AesCipher cipher(key, MBEDTLS_CIPHER_AES_128_CTR);
        cipher.SetIV(GetCipherIV(subsectionEntry, sectionOffset));

        const auto nextSubsection{GetNextSubsectionEntry(sectionOffset)};

        if (sectionOffset + output.size() > nextSubsection.addressPatch) {
            const u64 partition{nextSubsection.addressPatch - sectionOffset};
            span<u8> data(output.data() + partition, output.size() - partition);
            return ReadWithPartition(data, output.size() - partition, offset + partition) +
                ReadWithPartition(output, partition, offset);
        }

        const auto blockOffset{sectionOffset & 0xF};
        if (blockOffset != 0) {
            std::vector<u8> block(0x10);
            auto regionBacking{std::make_shared<RegionBacking>(bktrRomFs, sectionOffset & static_cast<u32>(~0xF), 0x10)};
            regionBacking->Read(block);

            cipher.Decrypt(block.data(), block.data(), block.size());
            if (output.size() + blockOffset < 0x10) {
                std::memcpy(output.data(), block.data() + blockOffset, std::min(output.size(), block.size()));
                return std::min(output.size(), block.size());
            }

            const auto read{0x10 - blockOffset};
            std::memcpy(output.data(), block.data() + blockOffset, read);
            span<u8> data(output.data() + read, output.size() - read);
            return read + ReadWithPartition(data, output.size() - read, offset + read);
        }

        auto regionBacking{std::make_shared<RegionBacking>(bktrRomFs, sectionOffset, output.size())};
        auto readSize{regionBacking->Read(output)};
        cipher.Decrypt(output.data(), output.data(), readSize);
        return readSize;
    }

    size_t BKTR::ReadWithPartition(span<u8> output, size_t length, size_t offset) {
        if (offset >= relocation.size)
            return 0;

        const auto relocationEntry{GetRelocationEntry(offset)};
        const auto sectionOffset{offset - relocationEntry.addressPatch + relocationEntry.addressSource};

        const auto nextRelocation{GetNextRelocationEntry(offset)};

        if (offset + length > nextRelocation.addressPatch) {
            const u64 partition{nextRelocation.addressPatch - offset};
            span<u8> data(output.data() + partition, length - partition);
            return ReadWithPartition(data, length - partition, offset + partition) + ReadWithPartition(output, partition, offset);
        }

       if (!relocationEntry.fromPatch) {
           span<u8> data(output.data(), length);
           auto regionBacking{std::make_shared<RegionBacking>(baseRomFs, sectionOffset - ivfcOffset, length)};
           return regionBacking->Read(data);
       }

       if (!isEncrypted)
           return bktrRomFs->Read(output, sectionOffset);

        const auto subsectionEntry{GetSubsectionEntry(sectionOffset)};

        crypto::AesCipher cipher(key, MBEDTLS_CIPHER_AES_128_CTR);
        cipher.SetIV(GetCipherIV(subsectionEntry, sectionOffset));

        const auto nextSubsection{GetNextSubsectionEntry(sectionOffset)};

        if (sectionOffset + length > nextSubsection.addressPatch) {
            const u64 partition{nextSubsection.addressPatch - sectionOffset};
            span<u8> data(output.data() + partition, length - partition);
            return ReadWithPartition(data, length - partition, offset + partition) +
                ReadWithPartition(output, partition, offset);
        }

        const auto blockOffset{sectionOffset & 0xF};
        if (blockOffset != 0) {
            std::vector<u8> block(0x10);
            auto regionBacking{std::make_shared<RegionBacking>(bktrRomFs, sectionOffset & static_cast<u32>(~0xF), 0x10)};
            regionBacking->Read(block);

            cipher.Decrypt(block.data(), block.data(), block.size());
            if (length + blockOffset < 0x10) {
                std::memcpy(output.data(), block.data() + blockOffset, std::min(length, block.size()));
                return std::min(length, block.size());
            }

            const auto read{0x10 - blockOffset};
            std::memcpy(output.data(), block.data() + blockOffset, read);
            span<u8> data(output.data() + read, length - read);
            return read + ReadWithPartition(data, length - read, offset + read);
        }

        auto regionBacking{std::make_shared<RegionBacking>(bktrRomFs, sectionOffset, length)};
        span<u8> data(output.data(), length);
        size_t readSize{0};
        if (length)
            readSize = regionBacking->Read(data);
        cipher.Decrypt(data.data(), data.data(), readSize);
        return readSize;
    }

    SubsectionEntry BKTR::GetNextSubsectionEntry(u64 offset) {
        const auto entry{SearchBucketEntry(offset, subsection, subsectionBuckets, true)};
        const auto bucket{subsectionBuckets[entry.first]};
        if (entry.second + 1 < bucket.entries.size())
            return bucket.entries[entry.second + 1];
        return subsectionBuckets[entry.first + 1].entries[0];
    }

    RelocationEntry BKTR::GetRelocationEntry(u64 offset) {
        const auto entry{SearchBucketEntry(offset, relocation, relocationBuckets, false)};
        return relocationBuckets[entry.first].entries[entry.second];
    }

    SubsectionEntry BKTR::GetSubsectionEntry(u64 offset) {
        const auto entry{SearchBucketEntry(offset, subsection, subsectionBuckets, true)};
        return subsectionBuckets[entry.first].entries[entry.second];
    }

    RelocationEntry BKTR::GetNextRelocationEntry(u64 offset) {
        const auto entry{SearchBucketEntry(offset, relocation, relocationBuckets, false)};
        const auto bucket{relocationBuckets[entry.first]};
        if (entry.second + 1 < bucket.entries.size())
            return bucket.entries[entry.second + 1];
        return relocationBuckets[entry.first + 1].entries[0];
    }

    std::array<u8, 16> BKTR::GetCipherIV(SubsectionEntry subsectionEntry, u64 sectionOffset) {
        std::array<u8, 16> iv{};
        auto subsectionCtr{subsectionEntry.ctr};
        auto offset_iv{sectionOffset + baseOffset};
        for (std::size_t i = 0; i < sectionCtr.size(); ++i) {
            iv[i] = sectionCtr[0x8 - i - 1];
        }
        offset_iv >>= 4;
        for (std::size_t i = 0; i < sizeof(u64); ++i) {
            iv[0xF - i] = static_cast<u8>(offset_iv & 0xFF);
            offset_iv >>= 8;
        }
        for (std::size_t i = 0; i < sizeof(u32); ++i) {
            iv[0x7 - i] = static_cast<u8>(subsectionCtr & 0xFF);
            subsectionCtr >>= 8;
        }
        return iv;
    }
}
