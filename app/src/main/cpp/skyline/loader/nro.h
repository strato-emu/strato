#pragma once

#include <cstdint>
#include "loader.h"

namespace skyline::loader {
    class NroLoader : public Loader {
      private:
        /**
         * @brief This holds a single data segment's offset and size
         */
        struct NroSegmentHeader {
            u32 offset; //!< The offset of the region
            u32 size; //!< The size of the region
        };

        /**
         * @brief This holds the header of an NRO file
         */
        struct NroHeader {
            u32 : 32;
            u32 modOffset; //!< The offset of the MOD metadata
            u64 : 64;

            u32 magic; //!< The NRO magic "NRO0"
            u32 version; //!< The version of the application
            u32 size; //!< The size of the NRO
            u32 flags; //!< The flags used with the NRO

            NroSegmentHeader text; //!< The .text segment header
            NroSegmentHeader ro; //!< The .ro segment header
            NroSegmentHeader data; //!< The .data segment header

            u32 bssSize; //!< The size of the bss segment
            u32 : 32;
            u64 buildId[4]; //!< The build ID of the NRO
            u64 : 64;

            NroSegmentHeader apiInfo; //!< The .apiInfo segment header
            NroSegmentHeader dynstr; //!< The .dynstr segment header
            NroSegmentHeader dynsym; //!< The .dynsym segment header
        } header{};

      public:
        /**
         * @param filePath The path to the ROM file
         */
        NroLoader(const int romFd);

        /**
         * This loads in the data of the main process
         * @param process The process to load in the data
         * @param state The state of the device
         */
        void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state);
    };
}
