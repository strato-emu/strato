#pragma once
#include <cstdint>

namespace core::loader {
    struct NroSegmentHeader {
        uint32_t fileOffset;
        uint32_t size;
    };

    struct NroHeader {
        uint32_t unused;
        uint32_t modOffset;
        uint64_t padding;

        uint32_t magic;
        uint32_t version;
        uint32_t size;
        uint32_t flags;

        NroSegmentHeader segments[3];

        uint32_t bssSize;
        uint32_t reserved0;
        uint64_t buildId[4];
        uint64_t reserved1;

        NroSegmentHeader extraSegments[3];
    };

    bool LoadNro(std::string filePath);
}