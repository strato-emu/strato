// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "aes_cipher.h"

namespace skyline::crypto {
    AesCipher::AesCipher(span<u8> key, mbedtls_cipher_type_t type) {
        mbedtls_cipher_init(&decryptContext);
        if (mbedtls_cipher_setup(&decryptContext, mbedtls_cipher_info_from_type(type)) != 0)
            throw exception("Failed to setup decryption context");

        if (mbedtls_cipher_setkey(&decryptContext, key.data(), key.size() * 8, MBEDTLS_DECRYPT) != 0)
            throw exception("Failed to set key for decryption context");
    }

    AesCipher::~AesCipher() {
        mbedtls_cipher_free(&decryptContext);
    }

    void AesCipher::SetIV(const std::array<u8, 0x10> &iv) {
        if (mbedtls_cipher_set_iv(&decryptContext, iv.data(), iv.size()) != 0)
            throw exception("Failed to set IV for decryption context");
    }

    void AesCipher::Decrypt(u8 *destination, u8 *source, size_t size) {
        std::optional<std::vector<u8>> buf{};

        u8 *targetDestination = [&]() {
            if (destination == source) {
                if (size > maxBufferSize) {
                    buf.emplace(size);
                    return buf->data();
                } else {
                    if (size > buffer.size())
                        buffer.resize(size);
                    return buffer.data();
                }
            }
            return destination;
        }();

        mbedtls_cipher_reset(&decryptContext);

        size_t outputSize{};
        if (mbedtls_cipher_get_cipher_mode(&decryptContext) == MBEDTLS_MODE_XTS) {
            mbedtls_cipher_update(&decryptContext, source, size, targetDestination, &outputSize);
        } else {
            u32 blockSize{mbedtls_cipher_get_block_size(&decryptContext)};

            for (size_t offset{}; offset < size; offset += blockSize) {
                size_t length{size - offset > blockSize ? blockSize : size - offset};
                mbedtls_cipher_update(&decryptContext, source + offset, length, targetDestination + offset, &outputSize);
            }
        }

        if (buf)
            std::memcpy(destination, buf->data(), size);
        else if (source == destination)
            std::memcpy(destination, buffer.data(), size);
    }

    void AesCipher::XtsDecrypt(u8 *destination, u8 *source, size_t size, size_t sector, size_t sectorSize) {
        if (size % sectorSize)
            throw exception("Size must be multiple of sector size");

        for (size_t i{}; i < size; i += sectorSize) {
            SetIV(GetTweak(sector++));
            Decrypt(destination + i, source + i, sectorSize);
        }
    }
}
