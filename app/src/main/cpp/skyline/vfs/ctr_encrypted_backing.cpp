// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ctr_encrypted_backing.h"

namespace skyline::vfs {
    constexpr size_t SectorSize{0x10};

    CtrEncryptedBacking::CtrEncryptedBacking(crypto::KeyStore::Key128 ctr, crypto::KeyStore::Key128 key, std::shared_ptr<Backing> backing, size_t baseOffset) : Backing({true, false, false}, backing->size), ctr(ctr), cipher(key, MBEDTLS_CIPHER_AES_128_CTR), backing(std::move(backing)), baseOffset(baseOffset) {
        if (mode.write || mode.append)
            throw exception("Cannot open a CtrEncryptedBacking as writable");
    }

    void CtrEncryptedBacking::UpdateCtr(u64 offset) {
        offset >>= 4;
        size_t le{util::SwapEndianness(offset)};
        std::memcpy(ctr.data() + 8, &le, 8);
        cipher.SetIV(ctr);
    }

    size_t CtrEncryptedBacking::ReadImpl(span<u8> output, size_t offset) {
        size_t size{output.size()};
        if (size == 0)
            return 0;

        size_t sectorOffset{offset % SectorSize};
        if (sectorOffset == 0) {
            size_t read{backing->ReadUnchecked(output, offset)};
            if (read != size)
                return 0;
            {
                std::lock_guard guard(mutex);
                UpdateCtr(baseOffset + offset);
                cipher.Decrypt(output);
            }
            return size;
        }

        size_t sectorStart{offset - sectorOffset};
        std::vector<u8> blockBuf(SectorSize);
        size_t read{backing->ReadUnchecked(blockBuf, sectorStart)};
        if (read != SectorSize)
            return 0;
        {
            std::lock_guard guard(mutex);
            UpdateCtr(baseOffset + sectorStart);
            cipher.Decrypt(blockBuf);
        }
        if (size + sectorOffset < SectorSize) {
            std::memcpy(output.data(), blockBuf.data() + sectorOffset, size);
            return size;
        }

        size_t readInBlock{SectorSize - sectorOffset};
        std::memcpy(output.data(), blockBuf.data() + sectorOffset, readInBlock);
        return readInBlock + ReadUnchecked(output.subspan(readInBlock), offset + readInBlock);
    }
}
