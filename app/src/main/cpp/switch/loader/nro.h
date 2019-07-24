#pragma once

#include <cstdint>
#include "loader.h"

namespace lightSwitch::loader {
    class NroLoader : public Loader {
    private:
        struct NroSegmentHeader {
            uint32_t offset;
            uint32_t size;
        };

        struct NroHeader {
            uint32_t : 32;
            uint32_t mod_offset;
            uint64_t : 64;

            uint32_t magic;
            uint32_t version;
            uint32_t size;
            uint32_t flags;

            NroSegmentHeader text;
            NroSegmentHeader ro;
            NroSegmentHeader data;

            uint32_t bssSize;
            uint32_t : 32;
            uint64_t build_id[4];
            uint64_t : 64;

            NroSegmentHeader api_info;
            NroSegmentHeader dynstr;
            NroSegmentHeader dynsym;
        };

        void Load(device_state state);

    public:
        NroLoader(std::string file_path, device_state state) : Loader(file_path, state) { Load(state); };
    };
}