#pragma once

#include <compare>
#include <common.h>

namespace skyline {
    /**
     * @brief Contains an RFC4122 BE format UUID
     */
    struct UUID {
        u128 raw{};

        /**
         * @brief Generates a random version 4 UUID
         */
        static UUID GenerateUuidV4();

        /**
         * @brief Generates a version 5 UUID from the given SHA1
         */
        static UUID GenerateUuidV5(span<u8, 20> sha1);

        /**
         * @brief Checks if a UUID is an invalid nil UUID
         */
        bool Valid() {
            return raw != 0;
        }

        auto operator<=>(const UUID &) const = default;
    };
}