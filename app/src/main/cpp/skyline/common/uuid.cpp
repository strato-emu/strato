#include <random>
#include "uuid.h"

namespace skyline {
    union UuidLayout {
        struct {
            u64 high;
            u64 low;
        };

        struct {
            u32 timeLow;
            u16 timeMid;
            union {
                u16 timeHighAndVersion;

                struct {
                    u16 timeHigh : 12;
                    u8 version : 4;
                };
            };

            union {
                u8 clockSeqHighAndReserved;

                struct {
                    u8 clockSeqHigh : 6;
                    u8 reserved : 2;
                };
            };
            u8 clockSeqLow;

            union __attribute__((packed)) {
                u64 node : 48;

                struct {
                    std::array<u8, 6> nodeArray;
                };
            };
        };

        UUID Swap() {
            UuidLayout swappedLayout{*this};
            swappedLayout.timeLow = util::SwapEndianness(timeLow);
            swappedLayout.timeMid = util::SwapEndianness(timeMid);
            swappedLayout.timeHighAndVersion = util::SwapEndianness(timeHighAndVersion);
            swappedLayout.nodeArray = util::SwapEndianness(nodeArray);

            UUID out;
            std::memcpy(&out, &swappedLayout, sizeof(UUID));
            return out;
        }
    };
    static_assert(sizeof(UuidLayout) == 0x10);

    UUID UUID::GenerateUuidV4() {
        constexpr u8 reserved{0x1}; // RFC4122 variant
        constexpr u8 version{0x4}; // v4 UUIDs are generated entirely from random numbers

        std::random_device rd;
        std::mt19937_64 gen(rd());

        std::uniform_int_distribution<u64> dist;

        // Create an initial random UUID
        UuidLayout uuid{
            .low = dist(gen),
            .high = dist(gen),
        };

        // Set format bits
        uuid.reserved = reserved;
        uuid.version = version;

        return uuid.Swap();
    }

    UUID UUID::GenerateUuidV5(span<u8, 20> sha1) {
        constexpr u8 reserved{0x1}; // RFC4122 variant
        constexpr u8 version{0x5}; // v4 UUIDs are generated using SHA1 hashes

        UuidLayout uuid;
        std::memcpy(&uuid, sha1.data(), sizeof(UuidLayout));

        // Set format bits
        uuid.reserved = reserved;
        uuid.version = version;

        return uuid.Swap();
    }
}
