// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/nacp.h>
#include "executable.h"

namespace skyline::loader {
    /**
     * @brief The types of ROM files
     * @note This needs to be synchronized with emu.skyline.loader.BaseLoader.RomFormat
     */
    enum class RomFormat {
        NRO, //!< The NRO format: https://switchbrew.org/wiki/NRO
        NSO, //!< The NSO format: https://switchbrew.org/wiki/NSO
        NCA, //!< The NCA format: https://switchbrew.org/wiki/NCA
        XCI, //!< The XCI format: https://switchbrew.org/wiki/XCI
        NSP, //!< The NSP format from "nspwn" exploit: https://switchbrew.org/wiki/Switch_System_Flaws
    };

    /**
     * @brief All possible results when parsing ROM files
     * @note This needs to be synchronized with emu.skyline.loader.LoaderResult
     */
    enum class LoaderResult : int8_t {
        Success,
        ParsingError,
        MissingHeaderKey,
        MissingTitleKey,
        MissingTitleKek,
        MissingKeyArea,
    };

    /**
     * @brief An exception used specifically for errors related to loaders, it's used to communicate errors to the Kotlin-side of the loader
     */
    class loader_exception : public exception {
      public:
        const LoaderResult error;

        loader_exception(LoaderResult error, const std::string &message = "No message") : exception("Loader exception {}: {}", error, message), error(error) {}
    };

    /**
     * @brief The Loader class provides an abstract interface for ROM loaders
     */
    class Loader {
      protected:
        /**
         * @brief Information about the placement of an executable in memory
         */
        struct ExecutableLoadInfo {
            u8* base; //!< The base of the loaded executable
            size_t size; //!< The total size of the loaded executable
            void* entry; //!< The entry point of the loaded executable
        };

        /**
         * @brief Loads an executable into memory
         * @param process The process to load the executable into
         * @param executable The executable itself
         * @param offset The offset from the base address that the executable should be placed at
         * @return An ExecutableLoadInfo struct containing the load base and size
         */
        static ExecutableLoadInfo LoadExecutable(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state, Executable &executable, size_t offset = 0);

      public:
        std::shared_ptr<vfs::NACP> nacp; //!< The NACP of the current application
        std::shared_ptr<vfs::Backing> romFs; //!< The RomFS of the current application

        virtual ~Loader() = default;

        virtual std::vector<u8> GetIcon() {
            return std::vector<u8>();
        }

        /**
         * @return Entry point to the start of the main executable in the ROM
         */
        virtual void* LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) = 0;
    };
}
