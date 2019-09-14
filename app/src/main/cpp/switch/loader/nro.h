#pragma once

#include <cstdint>
#include "loader.h"

namespace lightSwitch::loader {
    class NroLoader : public Loader {
      private:
        struct NroSegmentHeader {
            u32 offset;
            u32 size;
        }; //!< The structure of a single Segment descriptor in the NRO's header

        struct NroHeader {
            u32 : 32;
            u32 mod_offset;
            u64 : 64;

            u32 magic;
            u32 version;
            u32 size;
            u32 flags;

            NroSegmentHeader text;
            NroSegmentHeader ro;
            NroSegmentHeader data;

            u32 bssSize;
            u32 : 32;
            u64 build_id[4];
            u64 : 64;

            NroSegmentHeader api_info;
            NroSegmentHeader dynstr;
            NroSegmentHeader dynsym;
        }; //!< A bit-field struct to read the header of an NRO directly

      public:
        /**
         * @param file_path The path to the ROM file
         * @param state The state of the device
         */
        NroLoader(std::string file_path, const device_state &state);
    };
}
