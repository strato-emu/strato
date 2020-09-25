// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <array>
#include <span>
#include <mbedtls/cipher.h>
#include <common.h>

namespace skyline::crypto {
    /**
     * @brief Wrapper for mbedtls for AES decryption using a cipher
     */
    class AesCipher {
      private:
        mbedtls_cipher_context_t decryptContext;

        /**
         * @brief Buffer should grow bigger than 1 MiB
         */
        static constexpr size_t maxBufferSize = 1024 * 1024;

        /**
         * @brief Buffer declared as class variable to avoid constant memory allocation
         */
        std::vector<u8> buffer;

        /**
         * @brief Calculates IV for XTS, basically just big to little endian conversion.
         */
        inline static std::array<u8, 0x10> GetTweak(size_t sector) {
            std::array<u8, 0x10> tweak{};
            size_t le{__builtin_bswap64(sector)};
            std::memcpy(tweak.data() + 8, &le, 8);
            return tweak;
        }

      public:
        AesCipher(span<u8> key, mbedtls_cipher_type_t type);

        ~AesCipher();

        /**
         * @brief Sets initilization vector
         */
        void SetIV(const std::array<u8, 0x10> &iv);

        /**
         * @note destination and source can be the same
         */
        void Decrypt(u8 *destination, u8 *source, size_t size);

        /**
         * @brief Decrypts data and writes back to it
         */
        inline void Decrypt(span<u8> data) {
            Decrypt(data.data(), data.data(), data.size());
        }

        /**
         * @brief Decrypts data with XTS. IV will get calculated with the given sector
         */
        void XtsDecrypt(u8 *destination, u8 *source, size_t size, size_t sector, size_t sectorSize);

        /**
         * @brief Decrypts data with XTS and writes back to it
         */
        inline void XtsDecrypt(span<u8> data, size_t sector, size_t sectorSize) {
            XtsDecrypt(data.data(), data.data(), data.size(), sector, sectorSize);
        }
    };
}
