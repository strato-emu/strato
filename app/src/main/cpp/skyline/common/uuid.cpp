#include <random>
#include "uuid.h"

namespace skyline {
    namespace {
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
                        std::array<u8, 6> nodeRaw;
                    };
                };
            };

            UUID Swap() {
                UuidLayout swappedLayout{*this};
                swappedLayout.timeLow = util::SwapEndianness(timeLow);
                swappedLayout.timeMid = util::SwapEndianness(timeMid);
                swappedLayout.timeHighAndVersion = util::SwapEndianness(timeHighAndVersion);
                swappedLayout.nodeRaw = util::SwapEndianness(nodeRaw);

                UUID out;
                std::memcpy(&out, &swappedLayout, sizeof(UUID));
                return out;
            }
        };
        static_assert(sizeof(UuidLayout) == 0x10);
    }

    UUID UUID::GenerateUuidV4() {
        constexpr u8 reserved{0x1}; // RFC4122 variant
        constexpr u8 version{0x4}; // v4 UUIDs are generated entirely from random numbers

        std::random_device rd;
        std::mt19937_64 gen(rd());

        std::uniform_int_distribution <u64> dist;

        UuidLayout uuid;
        uuid.low = dist(gen);
        uuid.high = dist(gen);

        uuid.reserved = reserved;
        uuid.version = version;

        return uuid.Swap();
    }
}