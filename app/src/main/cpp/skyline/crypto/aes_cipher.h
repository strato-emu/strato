// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <mbedtls/cipher.h>
#include <common.h>

namespace skyline::crypto {
    /**
     * @brief Wrapper for mbedtls for AES decryption using a cipher
     * @note The IV state must be appropriately locked during multi-threaded usage
     */
    class AesCipher {
      private:
        mbedtls_cipher_context_t decryptContext;
        std::vector<u8> buffer; //!< A buffer used to avoid constant memory allocation

        /**
         * @brief Calculates IV for XTS, basically just big to little endian conversion
         */
        static std::array<u8, 0x10> GetTweak(size_t sector) {
            std::array<u8, 0x10> tweak{};
            size_t le{util::SwapEndianness(sector)};
            std::memcpy(tweak.data() + 8, &le, 8);
            return tweak;
        }

      public:
        AesCipher(span<u8> key, mbedtls_cipher_type_t type);

        ~AesCipher();

        /**
         * @brief Sets the Initialization Vector
         */
        void SetIV(const std::array<u8, 0x10> &iv);

        /**
         * @brief Decrypts the supplied buffer and outputs the result into the destination buffer
         * @note The destination and source buffers can be the same
         */
        void Decrypt(u8 *destination, u8 *source, size_t size);

        /**
         * @brief Decrypts the supplied data in-place
         */
        void Decrypt(span<u8> data) {
            Decrypt(data.data(), data.data(), data.size());
        }

        /**
         * @brief Decrypts data with XTS, IV will get calculated with the given sector
         */
        void XtsDecrypt(u8 *destination, u8 *source, size_t size, size_t sector, size_t sectorSize);

        /**
         * @brief Decrypts data with XTS and writes back to it
         */
        void XtsDecrypt(span<u8> data, size_t sector, size_t sectorSize) {
            XtsDecrypt(data.data(), data.data(), data.size(), sector, sectorSize);
        }
    };
}
