#pragma once

#include <cstdint>
#include "loader.h"

namespace skyline::loader {
    class NroLoader : public Loader {
      private:
        /**
         * @brief The structure of a single Segment descriptor in the NRO's header
         */
        struct NroSegmentHeader {
            u32 offset;
            u32 size;
        };

        /**
         * @brief A bit-field struct to read the header of an NRO directly
         */
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
        } header {};

      public:
        /**
         * @param filePath The path to the ROM file
         */
        NroLoader(std::string filePath);

        /**
         * This loads in the data of the main process
         * @param process The process to load in the data
         * @param state The state of the device
         */
        void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state);
    };
}
