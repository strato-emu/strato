// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <vfs/backing.h>
#include <vfs/nacp.h>

namespace skyline::loader {
    /**
     * @brief This enumerates the types of ROM files
     * @note This needs to be synchronized with emu.skyline.loader.BaseLoader.RomFormat
     */
    enum class RomFormat {
        NRO, //!< The NRO format: https://switchbrew.org/wiki/NRO
        XCI, //!< The XCI format: https://switchbrew.org/wiki/XCI
        NSP, //!< The NSP format from "nspwn" exploit: https://switchbrew.org/wiki/Switch_System_Flaws
    };

    /**
     * @brief The Loader class provides an abstract interface for ROM loaders
     */
    class Loader {
      protected:
        std::shared_ptr<vfs::Backing> backing; //!< The backing of the loader

      public:
        std::shared_ptr<vfs::NACP> nacp; //!< The NACP of the current application

        /**
         * @param backing The backing for the NRO
         */
        Loader(const std::shared_ptr<vfs::Backing> &backing) : backing(backing) {}

        virtual ~Loader() = default;

        /**
         * @return The icon of the loaded application
         */
        virtual std::vector<u8> GetIcon() {
            return std::vector<u8>();
        }

        /**
         * @brief This loads in the data of the main process
         * @param process The process to load in the data
         * @param state The state of the device
         */
        virtual void LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) = 0;
    };
}
